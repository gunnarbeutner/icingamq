#include <stdlib.h>
#include <assert.h>
#include "libimq.h"

imq_socket_t *imq_connect(const char *host, unsigned short port,
    const char *username, const char *password) {
	imq_socket_t *socket;

	assert(host != NULL);
	assert(username != NULL);
	assert(password != NULL);

	socket = (imq_socket_t *)malloc(sizeof (imq_socket_t));

	if (socket == NULL)
		return NULL;

	memset(socket, 0, sizeof (*socket));

	socket->fd = -1;
	socket->type = IMQ_CLIENT;
	socket->channels_head = NULL;

	socket->host = strdup(host);
	
	if (socket->host == NULL) {
		free(socket);

		return NULL;
	}

	socket->port = port;

	socket->username = strdup(username);

	if (socket->username == NULL) {
		free(socket->host);
		free(socket);

		return NULL;
	}

	socket->password = strdup(password);
	
	if (socket->password == NULL) {
		free(socket->username);
		free(socket->host);
		free(socket);

		return NULL;
	}

	/* TODO: start thread for this socket */
}

void *imq_open_channel(imq_socket_t *socket, const char *channel,
    const char *instance, int zmqtype) {

}

void imq_close(imq_socket_t *socket) {
	if (socket == NULL)
		return;

	if (socket->fd != -1)
		close(socket->fd);

	free(socket->password);
	free(socket->username);
	free(socket->host);
	free(socket);
}

imq_listener_t *imq_listener(unsigned short port, imq_authn_getpw_cb getpw_cb,
    imq_authz_channel_cb authz_channel_cb) {
	
}

void *imq_create_channel(imq_listener_t *listener, const char *channel,
    const char *instance, int zmqtype) {
	
}

int imq_device(imq_listener_t *downstream, imq_socket_t *upstream,
    int zmqdevice) {
	
}