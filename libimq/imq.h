#ifndef LIBIMQ_H
#define LIBIMQ_H

typedef int (*imq_authn_getpw_cb)(const char *username, char **password);
typedef int (*imq_authz_channel_cb)(const char *username, const char *channel);

typedef struct imq_endpoint_s {
	char *channel;
	char *instance;

	char *path;

	int listenerfd;
	void *zmqsocket;
} imq_endpoint_t;

typedef struct imq_listener_s {
	int listenerfd;

	imq_endpoint_t *endpoints;
	int endpointcount;

	imq_authn_getpw_cb getpw_cb;
	imq_authz_channel_cb authz_channel_cb;

	pthread_t iothread;
	pthread_mutex_t mutex;
} imq_listener_t;

typedef enum imq_socket_type_e {
	IMQ_SERVER,
	IMQ_CLIENT
} imq_socket_type_t;

typedef struct imq_queue_s {
	void *data;
	size_t size;
} imq_queue_t;

typedef struct imq_circuit_s {
	int id;
	int fd;
	/*imq_endpoint_t *endpoint;*/
} imq_circuit_t;

typedef struct imq_socket_s {
	int closed;

	int fd;

	imq_socket_type_t type;
	imq_listener_t *listener;

	char *host;
	unsigned short port;
	char *username;
	char *password;

	imq_endpoint_t **endpoints;
	int endpointcount;

	imq_circuit_t **circuits;
	int circuitcount;

	imq_queue_t sendq;
	imq_queue_t recvq;

	int has_iothread;
	pthread_t iothread;
	pthread_mutex_t mutex;
} imq_socket_t;

typedef enum imq_message_type_e {
	IMQ_MSG_RESULT,
	IMQ_MSG_AUTH_CHALLENGE,
	IMQ_MSG_AUTH_RESPONSE,
	IMQ_MSG_OPEN_CIRCUIT,
	IMQ_MSG_CLOSE_CIRCUIT,
	IMQ_MSG_DATA,
	IMQ_ADV_USER,
	IMQ_ADV_USER_COMMIT,
	IMQ_ADV_CHANNEL
} imq_message_type_t;

/* logging functions */
void imq_log(const char *format, ...);

/* low-level endpoint functions */
imq_endpoint_t *imq_alloc_endpoint(const char *channel, const char *instance);
void imq_free_endpoint(imq_endpoint_t *endpoint);
int imq_bind_zmq_endpoint(imq_endpoint_t *endpoint, void *zmqcontext,
    int zmqtype);
int imq_bind_unix_endpoint(imq_endpoint_t *endpoint);

/* socket functions */
imq_socket_t *imq_connect(const char *host, unsigned short port,
    const char *username, const char *password);
void *imq_open_zmq(imq_socket_t *socket, const char *channel,
    const char *instance, void *zmqcontext, int zmqtype);
void imq_close(imq_socket_t *socket);

/* listener functions */
imq_listener_t *imq_listener(unsigned short port,
    imq_authn_getpw_cb authn_getpw_cb, imq_authz_channel_cb authz_channel_cb);
int imq_listener_attach_endpoint(imq_listener_t *listener,
    imq_endpoint_t *endpoint);

/* broker functions */
int imq_run_broker(imq_listener_t *downstream, imq_socket_t *upstream);

#endif /* LIBIMQ_H */
