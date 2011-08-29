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

static int imq_socket_attach_endpoint(imq_socket_t *socket,
    imq_endpoint_t *endpoint);

static imq_socket_t *imq_socket(int fd, imq_socket_type_t type) {
	imq_socket_t *socket;

	socket = (imq_socket_t *)malloc(sizeof (*socket));

	if (socket == NULL)
		return NULL;

	memset(socket, 0, sizeof (*socket));

	socket->fd = fd;
	socket->type = type;

	socket->recvq = imq_alloc_fifo();
	socket->sendq = imq_alloc_fifo();

	pthread_mutex_init(&(socket->mutex), NULL);

	return socket;
}

static int imq_reconnect_socket(imq_socket_t *sock) {
	struct hostent *hent;
	struct sockaddr_in sin;
	int fd, i;

	assert(sock->host != NULL);

	imq_clear_fifo(sock->recvq);
	imq_clear_fifo(sock->sendq);

	for (i = 0; i < sock->endpointcount; i++) {
		imq_close_all_circuits(sock->endpoints[i]);
	}

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

static void imq_process_open_circuit(imq_socket_t *sock,
    imq_msg_open_circuit_t *open_circuit) {
	imq_endpoint_t *endpoint, *clone_endpoint;
	imq_circuit_t *circuit;
	imq_msg_t rejectmsg;

	rejectmsg.type = IMQ_MSG_CLOSE_CIRCUIT;
	rejectmsg.content.close_circuit.circuitid = open_circuit->circuitid;

	endpoint = sock->authz_channel_cb(sock, open_circuit->channel,
	    open_circuit->instance);

	if (endpoint == NULL) {	
		imq_send_message(sock->sendq, &rejectmsg);

		return;
	}

	assert(strcmp(endpoint->channel, open_circuit->channel) == 0);
	assert(open_circuit->instance == NULL ||
	    strcmp(endpoint->instance, open_circuit->instance) == 0);

	clone_endpoint = imq_shallow_clone_endpoint(endpoint);

	if (clone_endpoint == NULL) {
		imq_send_message(sock->sendq, &rejectmsg);

		return;
	}

	if (imq_socket_attach_endpoint(sock, clone_endpoint) < 0) {
		imq_send_message(sock->sendq, &rejectmsg);

		return;
	}

	circuit = imq_alloc_circuit(open_circuit->circuitid, -1);

	if (circuit == NULL) {
		imq_send_message(sock->sendq, &rejectmsg);

		return;
	}

	if (imq_connect_circuit(circuit, clone_endpoint) < 0) {
		imq_send_message(sock->sendq, &rejectmsg);

		return;
	}

	if (imq_attach_circuit(clone_endpoint, circuit) < 0) {
		imq_send_message(sock->sendq, &rejectmsg);

		return;
	}
}

static void imq_process_close_circuit(imq_socket_t *sock,
    imq_msg_close_circuit_t *close_circuit) {
	int i, k, rc;
	imq_endpoint_t *endpoint;
	imq_circuit_t *circuit;

	for (i = 0; i < sock->endpointcount; i++) {
		endpoint = sock->endpoints[i];

		for (k = 0; k < endpoint->circuitcount; k++) {
			circuit = endpoint->circuits[k];

			if (circuit->id != close_circuit->circuitid)
				continue;

			imq_detach_circuit(endpoint, circuit);
			imq_free_circuit(circuit);

			break;
		}
	}
}

static void imq_process_data_circuit(imq_socket_t *sock,
    imq_msg_data_circuit_t *data_circuit) {
	int i, k, rc;
	imq_endpoint_t *endpoint;
	imq_circuit_t *circuit;
	size_t offset;

	for (i = 0; i < sock->endpointcount; i++) {
		endpoint = sock->endpoints[i];

		for (k = 0; k < endpoint->circuitcount; k++) {
			circuit = endpoint->circuits[k];

			if (circuit->id != data_circuit->circuitid)
				continue;

			offset = 0;

			while (offset < data_circuit->len) {
				rc = write(circuit->fd, data_circuit->data,
				    data_circuit->len);

				if (rc < 0) {
					imq_detach_circuit(endpoint,
					    circuit);
					imq_free_circuit(circuit);
					break;
				}

				offset += rc;
			}

			break;
		}
	}
}

static void imq_process_message(imq_socket_t *sock, imq_msg_t *msg) {
	imq_log("Received msg: %d\n", msg->type);

	switch (msg->type) {
	case IMQ_MSG_CLOSE_CIRCUIT:
		imq_process_close_circuit(sock, &(msg->content.close_circuit));
		break;
	case IMQ_MSG_DATA_CIRCUIT:
		imq_process_data_circuit(sock, &(msg->content.data_circuit));
		break;
	default:
		break;
	}

	if (sock->type == IMQ_CLIENT) {
		switch (msg->type) {
		case IMQ_MSG_AUTH_CHALLENGE:
		case IMQ_MSG_ADV_USER:
		case IMQ_MSG_ADV_USER_COMMIT:
		default:
			break;
		}
	} else {
		switch (msg->type) {
		case IMQ_MSG_AUTH_RESPONSE:
		case IMQ_MSG_OPEN_CIRCUIT:
			imq_process_open_circuit(sock,
			    &(msg->content.open_circuit));
			break;
		default:
			break;
		}
	}
}

static void *imq_socket_io_thread(void *psocket) {
	fd_set readfds, writefds, exceptfds;
	imq_socket_t *sock = (imq_socket_t *)psocket;
	int i, k, nfds, rc, fd;
	struct timeval tv;
	char buffer[512];
	imq_endpoint_t *endpoint;
	imq_circuit_t *circuit;
	imq_msg_t msg;
	imq_msg_t *pmsg;
	time_t now;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	if (sock->fd != -1)
		fcntl(sock->fd, F_SETFL, fcntl(sock->fd, F_GETFL) | O_NONBLOCK);

	while (1) {
		pthread_mutex_lock(&(sock->mutex));

		if (sock->fd == -1) {
			if (sock->type == IMQ_CLIENT) {
				time(&now);

				if (now - sock->last_reconnect > RECONNECT_INTERVAL) {
					(void) imq_reconnect_socket(sock);
					time(&(sock->last_reconnect));
				}
			} else {
				if (sock->disowned)
					imq_close_socket(sock);

				break;
			}
		}

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);

		nfds = 0;

		if (sock->fd != -1) {
			nfds = sock->fd;

			FD_SET(sock->fd, &readfds);

			if (imq_fifo_size(sock->sendq) > 0)
				FD_SET(sock->fd, &writefds);

			FD_SET(sock->fd, &exceptfds);
		}

		for (i = 0; i < sock->endpointcount; i++) {
			endpoint = sock->endpoints[i];

			if (endpoint->listenerfd > nfds)
				nfds = endpoint->listenerfd;

			FD_SET(endpoint->listenerfd, &readfds);

			for (k = 0; k < endpoint->circuitcount; k++) {
				if (endpoint->circuits[k] == NULL)
					continue;

				if (endpoint->circuits[k]->fd > nfds)
					nfds = endpoint->circuits[k]->fd;

				FD_SET(endpoint->circuits[k]->fd, &readfds);
				FD_SET(endpoint->circuits[k]->fd, &exceptfds);
			}
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
			if (FD_ISSET(sock->fd, &readfds)) {
				rc = imq_splice_fifo(sock->recvq, sock->fd,
				    FIFO_FROM_FD);

				if (rc <= 0 && errno != EAGAIN &&
				    errno != EWOULDBLOCK) {
					close(sock->fd);
					sock->fd = -1;
				}
			}

			if (FD_ISSET(sock->fd, &writefds)) {
				rc = imq_splice_fifo(sock->sendq, sock->fd,
				    FIFO_TO_FD);

				if (rc <= 0 && errno != EAGAIN &&
				    errno != EWOULDBLOCK) {
					close(sock->fd);
					sock->fd = -1;
				}
			}

			if (FD_ISSET(sock->fd, &exceptfds)) {
				close(sock->fd);
				sock->fd = -1;
			}
		}

		for (i = 0; i < sock->endpointcount; i++) {
			endpoint = sock->endpoints[i];

			if (FD_ISSET(endpoint->listenerfd, &readfds)) {
				fd = accept(endpoint->listenerfd, NULL, NULL);

				if (fd < 0)
					continue;

				if (sock->fd == -1) {
					close(fd);
					continue;
				}

				circuit = imq_alloc_circuit(-1, fd);

				if (circuit == NULL) {
					close(fd);
					continue;
				}

				rc = imq_attach_circuit(endpoint, circuit);

				if (rc < 0) {
					imq_free_circuit(circuit);
					continue;
				}

				msg.type = IMQ_MSG_OPEN_CIRCUIT;
				msg.content.open_circuit.channel = endpoint->channel;
				msg.content.open_circuit.instance = endpoint->instance;
				msg.content.open_circuit.circuitid = circuit->id;

				rc = imq_send_message(sock->sendq, &msg);

				if (rc < 0) {
					close(sock->fd);
					sock->fd = -1;
				}
			}

			for (k = 0; k < endpoint->circuitcount; k++) {
				circuit = endpoint->circuits[k];

				if (FD_ISSET(circuit->fd, &readfds)) {
					rc = read(circuit->fd, buffer,
					    sizeof (buffer));

					if (rc < 0 && errno != EAGAIN &&
					    errno != EWOULDBLOCK) {
						imq_free_circuit(circuit);
						endpoint->circuits[k] = NULL;
					}

					if (rc <= 0)
						continue;

					imq_log("Read %d bytes from circuit.\n", rc);

					msg.type = IMQ_MSG_DATA_CIRCUIT;
					msg.content.data_circuit.circuitid = circuit->id;
					msg.content.data_circuit.len = rc;
					msg.content.data_circuit.data = buffer;

					rc = imq_send_message(sock->sendq, &msg);

					if (rc < 0) {
						close(sock->fd);
						sock->fd = -1;
					}
				}

				if (FD_ISSET(circuit->fd, &exceptfds)) {
					imq_free_circuit(circuit);
					endpoint->circuits[k] = NULL;
				}
			}
		}

		while ((pmsg = imq_receive_message(sock->recvq)) != NULL) {
			imq_process_message(sock, pmsg);
			imq_free_message(pmsg);
		}

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

	socket->authn_getpw_cb = listener->authn_getpw_cb;
	socket->authz_channel_cb = listener->authz_channel_cb;

	if (imq_start_socket_io(socket) < 0) {
		imq_close_socket(socket);

		return NULL;
	}

	return socket;
}

imq_socket_t *imq_connect(const char *host, unsigned short port,
    const char *username, const char *password) {
	imq_socket_t *socket;

	socket = imq_socket(-1, IMQ_CLIENT);

	if (socket == NULL)
		return NULL;

	socket->host = strdup(host);

	if (socket->host == NULL) {
		imq_close_socket(socket);
		return NULL;
	}

	socket->port = port;

	socket->username = strdup(username);

	if (socket->username == NULL) {
		imq_close_socket(socket);
		return NULL;
	}

	socket->password = strdup(password);

	if (socket->password == NULL) {
		imq_close_socket(socket);
		return NULL;
	}

	if (imq_start_socket_io(socket) < 0) {
		imq_close_socket(socket);

		return NULL;
	}

	return socket;
}

void imq_disown_socket(imq_socket_t *socket) {
	pthread_mutex_lock(&(socket->mutex));
	socket->disowned = 1;
	pthread_mutex_unlock(&(socket->mutex));
}

void imq_close_socket(imq_socket_t *socket) {
	int i;

	if (socket->fd != -1)
		close(socket->fd);

	pthread_mutex_lock(&(socket->mutex));
	if (socket->has_iothread)
		pthread_cancel(socket->iothread);
	pthread_mutex_unlock(&(socket->mutex));

	for (i = 0; i < socket->endpointcount; i++) {
		imq_free_endpoint(socket->endpoints[i]);
	}

	free(socket->endpoints);

	pthread_mutex_destroy(&(socket->mutex));

	imq_free_fifo(socket->recvq);
	imq_free_fifo(socket->sendq);

	free(socket->host);
	free(socket->username);
	free(socket->password);
	free(socket);
}

static int imq_socket_attach_endpoint(imq_socket_t *socket,
    imq_endpoint_t *endpoint) {
	imq_endpoint_t **new_endpoints;

	assert(endpoint->path != NULL);

	new_endpoints = (imq_endpoint_t **)realloc(socket->endpoints,
	    sizeof (imq_endpoint_t *) * (socket->endpointcount + 1));

	if (new_endpoints == NULL) {
		return -1;
	}

	socket->endpointcount++;
	socket->endpoints = new_endpoints;

	socket->endpoints[socket->endpointcount - 1] = endpoint;

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

	pthread_mutex_lock(&(socket->mutex));
	endpoint = imq_alloc_endpoint(channel, instance);
	imq_bind_unix_endpoint(endpoint);
	imq_socket_attach_endpoint(socket, endpoint);
	pthread_mutex_unlock(&(socket->mutex));

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
