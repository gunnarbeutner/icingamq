#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <zmq.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "imq.h"

static int imq_start_listener_io(imq_listener_t *listener);

imq_listener_t *imq_listener(unsigned short port) {
	imq_listener_t *listener;
	struct sockaddr_in sin;
	int reuse = 1;

	listener = (imq_listener_t *)malloc(sizeof (*listener));

	if (listener == NULL)
		return NULL;

	memset(listener, 0, sizeof (*listener));

	listener->fd = socket(AF_INET, SOCK_STREAM, 0);

	if (listener->fd < 0) {
		imq_close_listener(listener);

		return NULL;
	}

	(void )setsockopt(listener->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));

	memset(&sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	if (bind(listener->fd, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
		imq_close_listener(listener);

		return NULL;
	}

	if (listen(listener->fd, SOMAXCONN) < 0) {
		imq_close_listener(listener);

		return NULL;
	}

	if (pthread_mutex_init(&(listener->mutex), NULL) < 0) {
		imq_close_listener(listener);

		return NULL;
	}

	imq_start_listener_io(listener);

	return listener;
}

static void *imq_listener_io_thread(void *plistener) {
	imq_listener_t *listener = (imq_listener_t *)plistener;
	imq_socket_t *socket;
	int fd;

	while ((fd = accept(listener->fd, NULL, NULL))) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		socket = imq_server_socket(fd, listener);

		if (socket == NULL) {
			close(fd);

			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

			continue;
		}

		/* TODO: clone endpoint list into new socket */

		imq_disown_socket(socket);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}

	return NULL;
}

static int imq_start_listener_io(imq_listener_t *listener) {
	int rc;

	assert(!listener->has_iothread);

	rc = pthread_create(&(listener->iothread), NULL, imq_listener_io_thread,
	    listener);

	if (rc < 0)
		return rc;

	listener->has_iothread = 1;

	return 0;
}

int imq_listener_attach_endpoint(imq_listener_t *listener,
    imq_endpoint_t *endpoint) {
	imq_endpoint_t **new_endpoints;

	assert(endpoint->zmqsocket != NULL);

	pthread_mutex_lock(&(listener->mutex));

	new_endpoints = (imq_endpoint_t **)realloc(listener->endpoints,
	    sizeof (imq_endpoint_t *) * (listener->endpointcount + 1));

	if (new_endpoints == NULL) {
		pthread_mutex_unlock(&(listener->mutex));
		return -1;
	}

	listener->endpoints = new_endpoints;
	listener->endpointcount++;
	listener->endpoints[listener->endpointcount - 1] = endpoint;

	pthread_mutex_unlock(&(listener->mutex));

	return 0;
}

imq_endpoint_t *imq_listener_find_endpoint(imq_listener_t *listener,
    const char *channel, const char *instance) {
	int i;
	imq_endpoint_t *endpoint;

	pthread_mutex_lock(&(listener->mutex));

	for (i = 0; i < listener->endpointcount; i++) {
		endpoint = listener->endpoints[i];

		if (strcmp(endpoint->channel, channel) == 0 &&
		    (instance == NULL && endpoint->instance == NULL ||
		    strcmp(endpoint->instance, instance) == 0)) {
			pthread_mutex_unlock(&(listener->mutex));
			return endpoint;
		}
	}

	pthread_mutex_unlock(&(listener->mutex));

	return NULL;
}

void *imq_listener_create_zmq_endpoint(imq_listener_t *listener,
    const char *channel, const char *instance, void *zmqcontext, int zmqtype) {
	imq_endpoint_t *endpoint;

	endpoint = imq_alloc_endpoint(channel, instance);

	if (endpoint == NULL)
		return NULL;

	if (imq_bind_zmq_endpoint(endpoint, zmqcontext, zmqtype) < 0) {
		imq_free_endpoint(endpoint);
		return NULL;
	}

	if (imq_listener_attach_endpoint(listener, endpoint) < 0) {
		imq_free_endpoint(endpoint);
		return NULL;
	}

	return endpoint->zmqsocket;
}

void imq_close_listener(imq_listener_t *listener) {
	int i;

	if (listener->fd != -1)
		close(listener->fd);

	if (listener->has_iothread) {
		pthread_cancel(listener->iothread);
		pthread_join(listener->iothread, NULL);
	}

	imq_listener_clear_endpoints(listener);
	imq_listener_clear_users(listener);

	pthread_mutex_destroy(&(listener->mutex));

	free(listener);
}

void imq_listener_clear_endpoints(imq_listener_t *listener) {
	int i;

	for (i = 0; i < listener->endpointcount; i++) {
		imq_free_endpoint(listener->endpoints[i]);
	}

	free(listener->endpoints);
	listener->endpoints = NULL;
	listener->endpointcount = 0;
}

void imq_listener_clear_users(imq_listener_t *listener) {
	int i;

	for (i = 0; i < listener->usercount; i++) {
		imq_free_user(listener->users[i]);
	}

	free(listener->users);
	listener->users = NULL;
	listener->usercount = 0;
}

int imq_listener_add_user(imq_listener_t *listener, imq_user_t *user) {
	imq_user_t **new_users;

	new_users = (imq_user_t **)realloc(listener->users,
	    (listener->usercount + 1) * sizeof (imq_user_t *));

	if (new_users == NULL)
		return -1;

	listener->users = new_users;
	listener->usercount++;
	listener->users[listener->usercount - 1] = user;

	return 0;
}

int imq_listener_allow_user(imq_listener_t *listener, const char *username,
    const char *channel) {
	int i;
	imq_user_t *user;

	user = imq_listener_find_user(listener, username);

	if (user == NULL)
		return -1;

	return imq_user_allow_channel(user, channel);
}

imq_user_t *imq_listener_find_user(imq_listener_t *listener,
    const char *username) {
	int i;
	imq_user_t *user;

	if (username == NULL)
		return NULL;

	for (i = 0; i < listener->usercount; i++) {
		user = listener->users[i];

		if (strcmp(user->username, username) != 0)
			continue;

		return user;
	}

	return NULL;
}