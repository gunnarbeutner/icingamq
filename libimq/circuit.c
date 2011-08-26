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

imq_circuit_t *imq_alloc_circuit(int fd) {
	imq_circuit_t *circuit;

	circuit = (imq_circuit_t *)malloc(sizeof (*circuit));

	if (circuit == NULL)
		return NULL;

	circuit->id = ++next_circuit_id;
	circuit->fd = fd;

	return circuit;
}

void imq_free_circuit(imq_circuit_t *circuit) {
	close(circuit->fd);
	free(circuit);
}
