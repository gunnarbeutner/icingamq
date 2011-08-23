#ifndef LIBIMQ_IMPL_H
#define LIBIMQ_IMPL_H

typedef struct imq_channel_s {
	struct imq_channel_s *next;

	char *name;
	char *instance;
	char *path;
	void *zmqsocket;
} imq_channel_t;

typedef struct imq_listener_s {
	int fd;

	imq_channel_t *channels_head;

	imq_authn_getpw_cb getpw_cb;
	imq_authz_channel_cb authz_channel_cb;
} imq_listener_t;

typedef enum imq_socket_type_e {
	IMQ_SERVER,
	IMQ_CLIENT
} imq_socket_type_t;

typedef struct imq_queue_s {
	void *data;
	size_t size;
} imq_queue_t;

typedef struct imq_socket_s {
	int fd;
	imq_socket_type_t type;

	imq_channel_t *channels_head;

	char *host;
	unsigned short port;
	char *username;
	char *password;

	imq_queue_t sendq;
	imq_queue_t recvq;
} imq_socket_t;

typedef enum imq_message_type_e {
	IMQ_MSG_RESULT,
	IMQ_MSG_AUTH_CHALLENGE,
	IMQ_MSG_AUTH_RESPONSE,
	IMQ_OPEN_CHANNEL,
	IMQ_CLOSE_CHANNEL,
	IMQ_DATA
} imq_message_type_t;

#endif /* LIBIMQ_IMPL_H */
