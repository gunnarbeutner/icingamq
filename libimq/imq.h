#ifndef LIBIMQ_H
#define LIBIMQ_H

#include "fifo.h"
#include "imqmessage.h"

typedef struct imq_circuit_s {
	int id;
	int fd;
} imq_circuit_t;

typedef struct imq_endpoint_s {
	char *channel;
	char *instance;

	char *path;

	int listenerfd;
	void *zmqsocket;
	int zmqtype;

	imq_circuit_t **circuits;
	int circuitcount;

	pthread_rwlock_t rwlock;
} imq_endpoint_t;

struct imq_socket_s;

typedef int (*imq_authn_checkpw_cb)(const char *username, const char *password);
typedef int (*imq_authz_endpoint_cb)(struct imq_socket_s *socket,
    imq_endpoint_t *endpoint);

typedef struct imq_listener_s {
	int fd;

	imq_authn_checkpw_cb authn_checkpw_cb;
	imq_authz_endpoint_cb authz_endpoint_cb;

	int has_iothread;
	pthread_t iothread;
	pthread_mutex_t mutex;

	imq_endpoint_t **endpoints;
	int endpointcount;
} imq_listener_t;

typedef enum imq_socket_type_e {
	IMQ_SERVER,
	IMQ_CLIENT
} imq_socket_type_t;

typedef struct imq_socket_s {
	time_t last_reconnect;
	time_t last_announcement;

	int fd;
	imq_listener_t *listener;

	imq_socket_type_t type;

	char *host;
	unsigned short port;
	char *username;
	char *password;

	imq_endpoint_t **endpoints;
	int endpointcount;

	imq_fifo_t *sendq;
	imq_fifo_t *recvq;

	int has_iothread;
	pthread_t iothread;
	pthread_mutex_t mutex;

	int disowned;
	int shutdown;
} imq_socket_t;

/* logging functions */
void imq_log(const char *format, ...);

/* low-level circuit functions */
imq_circuit_t *imq_alloc_circuit(int id, int fd);
void imq_free_circuit(imq_circuit_t *circuit);

/* low-level endpoint functions */
imq_endpoint_t *imq_alloc_endpoint(const char *channel, const char *instance);
void imq_free_endpoint(imq_endpoint_t *endpoint);
imq_endpoint_t *imq_shallow_clone_endpoint(imq_endpoint_t *endpoint);
int imq_bind_zmq_endpoint(imq_endpoint_t *endpoint, void *zmqcontext,
    int zmqtype);
int imq_bind_unix_endpoint(imq_endpoint_t *endpoint);
int imq_attach_circuit(imq_endpoint_t *endpoint, imq_circuit_t *circuit);
int imq_detach_circuit(imq_endpoint_t *endpoint, imq_circuit_t *circuit);

/* socket functions */
imq_socket_t *imq_connect(const char *host, unsigned short port,
    const char *username, const char *password);
imq_socket_t *imq_server_socket(int fd, imq_listener_t *listener);
void *imq_open_zmq(imq_socket_t *socket, const char *channel,
    const char *instance, void *zmqcontext, int zmqtype);
void imq_disown_socket(imq_socket_t *socket);
void imq_close_socket(imq_socket_t *socket);

/* low-level listener functions */
int imq_listener_attach_endpoint(imq_listener_t *listener,
    imq_endpoint_t *endpoint);
imq_endpoint_t *imq_listener_find_endpoint(imq_listener_t *listener,
    const char *channel, const char *instance);

/* listener functions */
imq_listener_t *imq_listener(unsigned short port,
    imq_authn_checkpw_cb authn_checkpw_cb,
    imq_authz_endpoint_cb authz_endpoint_cb);
void *imq_listener_create_zmq_endpoint(imq_listener_t *listener,
    const char *channel, const char *instance, void *zmqcontext, int zmqtype);
void imq_close_listener(imq_listener_t *listener);

/* broker functions */
int imq_run_broker(imq_listener_t *downstream, imq_socket_t *upstream);

#endif /* LIBIMQ_H */
