#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <zmq.h>
#include "imq.h"

static int next_circuit_id;

imq_circuit_t *imq_alloc_circuit(int id, int fd) {
	imq_circuit_t *circuit;

	circuit = (imq_circuit_t *)malloc(sizeof (*circuit));

	if (circuit == NULL)
		return NULL;

	if (id == -1)
		id = ++next_circuit_id;

	circuit->id = id;
	circuit->fd = fd;

	return circuit;
}

void imq_free_circuit(imq_circuit_t *circuit) {
	close(circuit->fd);
	free(circuit);
}

int imq_connect_circuit(imq_circuit_t *circuit, imq_endpoint_t *endpoint) {
	struct sockaddr_un sun;

	assert(circuit->fd == -1);
	assert(endpoint->path != NULL);

	circuit->fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (circuit->fd < 0) {
		circuit->fd = -1;

		return -1;
	}

	memset(&sun, 0, sizeof (sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, endpoint->path, sizeof(sun.sun_path) - 1);
	sun.sun_path[sizeof(sun.sun_path) - 1] = '\0';

	if (connect(circuit->fd, (struct sockaddr *)&sun, sizeof (sun)) < 0) {
		close(circuit->fd);
		circuit->fd = -1;

		return -1;
	}

	return 0;
}