#include <stdlib.h>
#include <string.h>
#include <zmq.h>
#include "imq.h"

typedef struct imq_broker_s {
	void *zmqcontext;
	imq_listener_t *downstream;
	imq_socket_t *upstream;
} imq_broker_t;

typedef struct imq_device_s {
	int type;
	void *frontend;
	void *backend;

	imq_endpoint_t **endpoints;
	int endpointcount;
} imq_device_t;

static void imq_broker_adv_authn_callback(imq_socket_t *sock, void *pbroker,
    imq_authn_event_t eventtype, const char *username, const char *password,
    int super, const char *channel) {
	imq_broker_t *broker = (imq_broker_t *)pbroker;
	imq_user_t *user;

	/* TODO: locking */

	switch (eventtype) {
	case IMQ_AUTHN_ROLLBACK:
		imq_listener_clear_users(broker->downstream);
		break;
	case IMQ_AUTHN_COMMIT:
		break;
	case IMQ_AUTHN_ADD:
		user = imq_alloc_user(username, password, super);

		if (user == NULL)
			break;

		(void) imq_listener_add_user(broker->downstream, user);

		break;
	case IMQ_AUTHN_ALLOW:
		user = imq_listener_find_user(broker->downstream, username);

		if (user == NULL)
			break;

		(void) imq_user_allow_channel(user, channel);

		break;
	}
}

static void imq_broker_io_thread(void *pdevice) {
	imq_device_t *device = (imq_device_t *)pdevice;

	zmq_device(device->type, device->frontend, device->backend);

	free(device);
}

static void imq_broker_adv_endpoint_callback(imq_socket_t *sock, void *pbroker,
    const char *channel, const char *instance, int zmqtype) {
	imq_broker_t *broker = (imq_broker_t *)pbroker;
	int zmqupstream_type, zmqdevice_type;
	pthread_t thread;
	imq_device_t *device;

	if (imq_listener_find_endpoint(broker->downstream, channel, instance) !=
	    NULL)
		return;

	switch (zmqtype) {
	case ZMQ_XREP:
		zmqupstream_type = ZMQ_XREQ;
		zmqdevice_type = ZMQ_QUEUE;
		break;
	case ZMQ_PUB:
		zmqupstream_type = ZMQ_SUB;
		zmqdevice_type = ZMQ_FORWARDER;
		break;
	case ZMQ_PUSH:
		zmqupstream_type = ZMQ_PULL;
		zmqdevice_type = ZMQ_STREAMER;
		break;
	default:
		return;
	}

	device = (imq_device_t *)malloc(sizeof (*device));

	if (device == NULL)
		return;

	device->type = zmqdevice_type;

	device->frontend = imq_open_zmq(sock, channel, instance,
	    broker->zmqcontext, zmqupstream_type);

	if (zmqtype == ZMQ_PUB)
		zmq_setsockopt(device->frontend, ZMQ_SUBSCRIBE, NULL, 0);

	device->backend = imq_listener_create_zmq_endpoint(broker->downstream,
	    channel, instance, broker->zmqcontext, zmqtype);

	if (pthread_create(&thread, NULL, imq_broker_io_thread, device) < 0)
		return;

	pthread_detach(thread);
}

int imq_run_broker(imq_listener_t *downstream, imq_socket_t *upstream) {
	imq_broker_t broker;

	memset(&broker, 0, sizeof (broker));

	broker.zmqcontext = zmq_init(1);
	broker.downstream = downstream;
	broker.upstream = upstream;

	imq_socket_set_callbacks(upstream, imq_broker_adv_authn_callback,
	    imq_broker_adv_endpoint_callback, &broker);

	while (1)
		sleep(10);
}
