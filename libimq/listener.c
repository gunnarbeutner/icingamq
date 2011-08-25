#include <stdlib.h>
#include <assert.h>
#include <zmq.h>
#include "imq.h"

imq_listener_t *imq_listener(unsigned short port, imq_authn_getpw_cb getpw_cb,
    imq_authz_channel_cb authz_channel_cb) {
	
}

int imq_listener_attach_endpoint(imq_listener_t *listener,
    imq_endpoint_t *endpoint) {
	assert(endpoint->zmqsocket != NULL);

	return -1;
}
