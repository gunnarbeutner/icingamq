#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <zmq.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "imq.h"

#define RECONNECT_INTERVAL 5

static imq_socket_t *imq_socket(int fd, imq_socket_type_t type) {
	imq_socket_t *socket;

	socket = (imq_socket_t *)malloc(sizeof (*socket));

	if (socket == NULL)
		return NULL;

	memset(socket, 0, sizeof (*socket));

	socket->fd = fd;
	socket->type = type;

	pthread_mutex_init(&(socket->mutex), NULL);

	return socket;
}

static int imq_reconnect_socket(imq_socket_t *sock) {
	struct hostent *hent;
	struct sockaddr_in sin;
	int fd, i;

	assert(sock->host != NULL);

	for (i = 0; i < sock->circuitcount; i++) {
		/* TODO: implement */
		/*imq_free_circuit(sock->circuits[i]);*/
	}

	free(sock->circuits);
	sock->circuits = NULL;
	sock->circuitcount = 0;

	hent = gethostbyname(sock->host);

	if (hent == NULL) {
		imq_log("Could not resolve hostname: %s\n",
		    sock->host);
		return -1;
	}

	memset(&sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	memcpy(&(sin.sin_addr.s_addr),
	    (struct in_addr *)hent->h_addr_list[0],
	    sizeof (struct in_addr));
	sin.sin_port = htons(sock->port);

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		imq_log("socket() failed: %s\n", strerror(errno));
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&sin,
	    sizeof (sin)) < 0) {
		imq_log("connect() failed: %s\n",
		    strerror(errno));
		close(fd);
		return -1;
	}

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

	sock->fd = fd;

	return 0;
}
static void *imq_socket_io_thread(void *psocket) {
	fd_set readfds, writefds, exceptfds;
	imq_socket_t *sock = (imq_socket_t *)psocket;
	int i, nfds, rc, fd;
	struct timeval tv;
	void *new_data;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	if (sock->fd != -1)
		fcntl(sock->fd, F_SETFL, fcntl(sock->fd, F_GETFL) | O_NONBLOCK);

	while (1) {
		pthread_mutex_lock(&(sock->mutex));

		if (sock->type == IMQ_CLIENT && sock->fd == -1)
			(void) imq_reconnect_socket(sock);

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);

		nfds = 0;

		if (sock->fd != -1) {
			nfds = sock->fd;

			FD_SET(sock->fd, &readfds);

			if (sock->sendq.size > 0)
				FD_SET(sock->fd, &writefds);

			FD_SET(sock->fd, &exceptfds);
		}

		for (i = 0; i < sock->endpointcount; i++) {
			if (sock->endpoints[i]->listenerfd > nfds)
				nfds = sock->endpoints[i]->listenerfd;

			FD_SET(sock->endpoints[i]->listenerfd, &readfds);
		}

		pthread_mutex_unlock(&(sock->mutex));
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		tv.tv_sec = 5;
		tv.tv_usec = 0;

		rc = select(nfds + 1, &readfds, &writefds, &exceptfds, &tv);

		if (rc < 0)
			return NULL;
		else if (rc == 0)
			continue;

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		pthread_mutex_lock(&(sock->mutex));

		if (sock->fd != -1) {
			if (FD_ISSET(sock->fd, &writefds)) {
				rc = write(sock->fd, sock->sendq.data,
				    sock->sendq.size);

				if (rc < 0 && errno != EAGAIN &&
				    errno != EWOULDBLOCK) {
					close(sock->fd);
					sock->fd = -1;
				}

				if (rc <= 0) {
					pthread_mutex_unlock(&(sock->mutex));
					continue;
				}

				new_data = malloc(sock->sendq.size -
				    rc);

				if (new_data == NULL) {
					close(sock->fd);
					sock->fd = -1;

					pthread_mutex_unlock(&(sock->mutex));
					continue;
				}

				memcpy(new_data,
				    ((char *)sock->sendq.data) + rc,
				    sock->sendq.size - rc);

				free(sock->sendq.data);

				sock->sendq.data = new_data;
				sock->sendq.size -= rc;
			}
			if (FD_ISSET(sock->fd, &exceptfds)) {
				close(sock->fd);
				sock->fd = -1;

				pthread_mutex_unlock(&(sock->mutex));
				continue;
			}
		}

		for (i = 0; i < sock->endpointcount; i++) {
			if (FD_ISSET(sock->endpoints[i]->listenerfd,
			    &readfds)) {
				fd = accept(sock->endpoints[i]->listenerfd,
				    NULL, NULL);

				if (fd < 0)
					continue;

				if (sock->fd == -1) {
					close(fd);
					continue;
				}

				/* TODO: create circuit, send connect
				 * request upstream */
			}
		}

		/* TODO: process recvq */

		pthread_mutex_unlock(&(sock->mutex));
	}

	return NULL;
}

static int imq_start_socket_io(imq_socket_t *socket) {
	int rc;

	assert(!socket->has_iothread);

	rc = pthread_create(&(socket->iothread), NULL, imq_socket_io_thread,
	    socket);

	if (rc < 0)
		return rc;

	socket->has_iothread = 1;

	return 0;
}

imq_socket_t *imq_server_socket(int fd, imq_listener_t *listener) {
	imq_socket_t *socket;

	socket = imq_socket(fd, IMQ_SERVER);

	if (socket == NULL)
		return NULL;

	socket->listener = listener;

	if (imq_start_socket_io(socket) < 0) {
		imq_close(socket);

		return NULL;
	}

	return NULL;
}

imq_socket_t *imq_connect(const char *host, unsigned short port,
    const char *username, const char *password) {
	imq_socket_t *socket;

	socket = imq_socket(-1, IMQ_CLIENT);

	if (socket == NULL)
		return NULL;

	socket->host = strdup(host);

	if (socket->host == NULL) {
		imq_close(socket);
		return NULL;
	}

	socket->port = port;

	socket->username = strdup(username);

	if (socket->username == NULL) {
		imq_close(socket);
		return NULL;
	}

	socket->password = strdup(password);

	if (socket->password == NULL) {
		imq_close(socket);
		return NULL;
	}

	if (imq_start_socket_io(socket) < 0) {
		imq_close(socket);

		return NULL;
	}

	return socket;
}

void imq_close(imq_socket_t *socket) {
	int i;

	if (socket->fd != -1)
		close(socket->fd);

	pthread_mutex_lock(&(socket->mutex));
	socket->closed = 1;
	if (socket->has_iothread)
		pthread_cancel(socket->iothread);
	pthread_mutex_unlock(&(socket->mutex));

	for (i = 0; i < socket->endpointcount; i++) {
		imq_free_endpoint(socket->endpoints[i]);
	}

	pthread_mutex_destroy(&(socket->mutex));

	free(socket->host);
	free(socket->username);
	free(socket->password);
	free(socket);
}

int imq_socket_attach_endpoint(imq_socket_t *socket, imq_endpoint_t *endpoint) {
	imq_endpoint_t **new_endpoints;

	assert(endpoint->listenerfd != -1);

	pthread_mutex_lock(&(socket->mutex));

	new_endpoints = (imq_endpoint_t **)realloc(socket->endpoints,
	    sizeof (imq_endpoint_t *) * (socket->endpointcount + 1));

	if (new_endpoints == NULL) {
		pthread_mutex_unlock(&(socket->mutex));
		return -1;
	}

	socket->endpointcount++;
	socket->endpoints = new_endpoints;

	socket->endpoints[socket->endpointcount - 1] = endpoint;

	pthread_mutex_unlock(&(socket->mutex));

	return 0;
}

void *imq_open_zmq(imq_socket_t *socket, const char *channel,
    const char *instance, void *zmqcontext, int zmqtype) {
	imq_endpoint_t *endpoint;
	char *socket_addr;
	void *zmqsocket;
	int rc;

	assert(socket != NULL);
	assert(channel != NULL);

	endpoint = imq_alloc_endpoint(channel, instance);
	imq_bind_unix_endpoint(endpoint);
	imq_socket_attach_endpoint(socket, endpoint);

	zmqsocket = zmq_socket(zmqcontext, zmqtype);

	rc = asprintf(&socket_addr, "ipc://%s", endpoint->path);

	if (rc < 0) {
		zmq_close(zmqsocket);
		return NULL;
	}

	rc = zmq_connect(zmqsocket, socket_addr);

	free(socket_addr);

	if (rc < 0) {
		zmq_close(zmqsocket);
		return NULL;
	}

	return zmqsocket;
}
