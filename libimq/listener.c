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

imq_listener_t *imq_listener(unsigned short port,
    imq_authn_getpw_cb authn_getpw_cb, imq_authz_channel_cb authz_channel_cb) {
	imq_listener_t *listener;
	struct sockaddr_in sin;
	int reuse = 1;

	listener = (imq_listener_t *)malloc(sizeof (*listener));

	if (listener == NULL)
		return NULL;

	memset(listener, 0, sizeof (*listener));

	listener->authn_getpw_cb = authn_getpw_cb;
	listener->authz_channel_cb = authz_channel_cb;

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

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	while ((fd = accept(listener->fd, NULL, NULL))) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		socket = imq_server_socket(fd, listener);

		if (socket == NULL) {
			close(fd);

			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

			continue;
		}

		/* TODO: clone endpoint list into new socket */

		imq_disown_socket(socket);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
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
	assert(endpoint->zmqsocket != NULL);

	return -1;
}

imq_endpoint_t *imq_listener_find_endpoint(imq_listener_t *listener,
    const char *channel, const char *instance) {
	return NULL;
}

void imq_close_listener(imq_listener_t *listener) {
	int i;

	if (listener->fd != -1)
		close(listener->fd);

	pthread_mutex_lock(&(listener->mutex));
	if (listener->has_iothread)
		pthread_cancel(listener->iothread);
	pthread_mutex_unlock(&(listener->mutex));

	pthread_mutex_destroy(&(listener->mutex));

	free(listener);
}
