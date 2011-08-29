#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <zmq.h>
#include <pthread.h>
#include "imq.h"

imq_endpoint_t *imq_alloc_endpoint(const char *channel, const char *instance) {
	imq_endpoint_t *endpoint;

	assert(channel != NULL);

	endpoint = (imq_endpoint_t *)malloc(sizeof (*endpoint));

	if (endpoint == NULL)
		return NULL;

	memset(endpoint, 0, sizeof (*endpoint));

	endpoint->channel = strdup(channel);

	if (endpoint->channel == NULL) {
		free(endpoint);
		return NULL;
	}

	if (instance != NULL) {
		endpoint->instance = strdup(instance);

		if (endpoint->instance == NULL) {
			free(endpoint->channel);
			free(endpoint);
			return NULL;
		}
	}

	endpoint->listenerfd = -1;

	return endpoint;
}

void imq_free_endpoint(imq_endpoint_t *endpoint) {
	if (endpoint == NULL)
		return;

	free(endpoint->channel);
	free(endpoint->instance);

	if (endpoint->zmqsocket != NULL)
		zmq_close(endpoint->zmqsocket);

	if (endpoint->listenerfd != -1)
		close(endpoint->listenerfd);

	if (endpoint->path != NULL) {
		(void) unlink(endpoint->path);
		free(endpoint->path);
	}

	free(endpoint);
}

int imq_bind_zmq_endpoint(imq_endpoint_t *endpoint, void *zmqcontext,
    int zmqtype) {
	char template[] = "/tmp/imq.XXXXXX";
	void *sock;
	char *sock_addr;

	assert(endpoint->path == NULL);
	assert(endpoint->listenerfd == -1);
	assert(endpoint->zmqsocket == NULL);

	mktemp(template);

	sock = zmq_socket(zmqcontext, zmqtype);

	if (sock == NULL)
		return -1;

	if (asprintf(&sock_addr, "ipc://%s", template) < 0) {
		zmq_close(sock);
		return -1;
	}

	if (zmq_bind(sock, sock_addr) < 0) {
		zmq_close(sock);
		return -1;
	}

	free(sock_addr);

	endpoint->path = strdup(template);

	if (endpoint->path == NULL) {
		zmq_close(sock);
		return -1;
	}

	endpoint->zmqsocket = sock;

	return 0;
}

int imq_bind_unix_endpoint(imq_endpoint_t *endpoint) {
	assert(endpoint->path == NULL);
	assert(endpoint->listenerfd == -1);
	assert(endpoint->zmqsocket == NULL);

	while (1) {
		char template[] = "/tmp/imq.XXXXXX";
		struct sockaddr_un sun;
		int fd;

		mktemp(template);

		fd = socket(AF_UNIX, SOCK_STREAM, 0);

		if (fd < 0)
			return -1;

		sun.sun_family = AF_UNIX;
		strncpy(sun.sun_path, template, sizeof (sun.sun_path) - 1);
		sun.sun_path[sizeof (sun.sun_path) - 1] = '\0';

		if (bind(fd, (struct sockaddr *)&sun, sizeof (sun)) < 0) {
			close(fd);

			continue;
		}

		if (listen(fd, SOMAXCONN) < 0) {
			close(fd);

			return -1;
		}

		endpoint->path = strdup(template);

		if (endpoint->path == NULL) {
			close(fd);

			return -1;
		}

		endpoint->listenerfd = fd;

		break;
	}

	return 0;
}

int imq_attach_circuit(imq_endpoint_t *endpoint, imq_circuit_t *circuit) {
	int i;
	imq_circuit_t **new_circuits;

	for (i = 0; i < endpoint->circuitcount; i++) {
		if (endpoint->circuits[i] == NULL) {
			endpoint->circuits[i] = circuit;
			return 0;
		}
	}

	new_circuits = (imq_circuit_t **)realloc(endpoint->circuits,
	    sizeof (imq_circuit_t *) * (endpoint->circuitcount + 1));

	if (new_circuits == NULL)
		return -1;

	endpoint->circuits = new_circuits;
	endpoint->circuitcount++;

	endpoint->circuits[endpoint->circuitcount - 1] = circuit;

	return 0;
}

int imq_detach_circuit(imq_endpoint_t *endpoint, imq_circuit_t *circuit) {
	int i;

	for (i = 0; i < endpoint->circuitcount; i++) {
		if (endpoint->circuits[i] == circuit)
			endpoint->circuits[i] = NULL;
	}

	return 0;
}

int imq_close_all_circuits(imq_endpoint_t *endpoint) {
	int i;

	for (i = 0; i < endpoint->circuitcount; i++)
		imq_free_circuit(endpoint->circuits[i]);

	free(endpoint->circuits);
	endpoint->circuits = NULL;
	endpoint->circuitcount = 0;

	return 0;
}

imq_endpoint_t *imq_shallow_clone_endpoint(imq_endpoint_t *endpoint) {
	imq_endpoint_t *clone_endpoint;

	clone_endpoint = (imq_endpoint_t *)malloc(sizeof (*clone_endpoint));

	if (clone_endpoint == NULL)
		return NULL;

	memset(clone_endpoint, 0, sizeof (*clone_endpoint));

	clone_endpoint->channel = strdup(endpoint->channel);

	if (clone_endpoint->channel == NULL) {
		free(clone_endpoint);

		return NULL;
	}

	if (endpoint->instance != NULL) {
		clone_endpoint->instance = strdup(endpoint->instance);

		if (clone_endpoint->instance == NULL) {
			free(clone_endpoint->channel);
			free(clone_endpoint);

			return NULL;
		}
	}

	clone_endpoint->path = strdup(endpoint->path);

	if (clone_endpoint->path == NULL) {
		free(clone_endpoint->instance);
		free(clone_endpoint->channel);
		free(clone_endpoint);

		return NULL;
	}

	clone_endpoint->listenerfd = -1;

	return clone_endpoint;
}
