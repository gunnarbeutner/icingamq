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
	int owns_path;

	int listenerfd;
	void *zmqsocket;
	int zmqtype;

	imq_circuit_t **circuits;
	int circuitcount;
} imq_endpoint_t;

typedef struct imq_user_s {
	char *username;
	char *password;
	int super;

	char **channels;
	int channelcount;
} imq_user_t;

struct imq_socket_s;

typedef enum imq_authn_event_e {
	IMQ_AUTHN_ROLLBACK,
	IMQ_AUTHN_COMMIT,
	IMQ_AUTHN_ADD,
	IMQ_AUTHN_ALLOW
} imq_authn_event_t;

typedef void (*imq_adv_authn_cb_t)(struct imq_socket_s *sock, void *cookie,
    imq_authn_event_t eventtype, const char *username, const char *password,
    int super, const char *channel);
typedef void (*imq_adv_endpoint_cb_t)(struct imq_socket_s *sock, void *cookie,
    const char *channel, const char *instance, int zmqtype);

typedef struct imq_listener_s {
	int fd;

	int has_iothread;
	pthread_t iothread;
	pthread_mutex_t mutex;

	imq_endpoint_t **endpoints;
	int endpointcount;

	imq_user_t **users;
	int usercount;
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

	imq_adv_authn_cb_t adv_authn_cb;
	imq_adv_endpoint_cb_t adv_endpoint_cb;
	void *callback_cookie;

	char *host;
	unsigned short port;

	imq_user_t *user;
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
void imq_socket_set_callbacks(imq_socket_t *sock,
    imq_adv_authn_cb_t adv_authn_cb, imq_adv_endpoint_cb_t adv_endpoint_cb,
    void *cookie);
imq_socket_t *imq_server_socket(int fd, imq_listener_t *listener);
void *imq_open_zmq(imq_socket_t *socket, const char *channel,
    const char *instance, void *zmqcontext, int zmqtype);
void imq_disown_socket(imq_socket_t *socket);
void imq_close_socket(imq_socket_t *socket);

/* low-level listener functions */
int imq_listener_attach_endpoint(imq_listener_t *listener,
    imq_endpoint_t *endpoint);
void imq_listener_clear_endpoints(imq_listener_t *listener);
imq_endpoint_t *imq_listener_find_endpoint(imq_listener_t *listener,
    const char *channel, const char *instance);

/* listener functions */
imq_listener_t *imq_listener(unsigned short port);
void *imq_listener_create_zmq_endpoint(imq_listener_t *listener,
    const char *channel, const char *instance, void *zmqcontext, int zmqtype);
void imq_close_listener(imq_listener_t *listener);

/* listener authentication functions */
void imq_listener_clear_users(imq_listener_t *listener);
int imq_listener_add_user(imq_listener_t *listener, imq_user_t *user);
int imq_listener_allow_user(imq_listener_t *listener, const char *username,
    const char *channel);
imq_user_t *imq_listener_find_user(imq_listener_t *listener,
    const char *username);

/* low-level user functions */
imq_user_t *imq_alloc_user(const char *username, const char *password,
    int super);
void imq_free_user(imq_user_t *user);
int imq_user_allow_channel(imq_user_t *user, const char *channel);
int imq_user_authz_endpoint(imq_user_t *user, imq_endpoint_t *endpoint);

/* broker functions */
int imq_run_broker(imq_listener_t *downstream, imq_socket_t *upstream);

#endif /* LIBIMQ_H */
