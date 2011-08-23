typedef int (*imq_getpw_cb)(const char *username, char **password);

typedef struct imq_channel_s {
	struct imq_channel_s *next;

	char *name;
	void *zmqsocket;
} imq_channel_t;

typedef struct imq_socket_s {
	int fd;

	imq_channel_t *channels_head;

	imq_getpw_cb get_pass_cb;

	char *username;
	char *password;
} imq_socket_t;

imq_socket_t *imq_socket(void);
void imq_close(imq_socket_t *socket);

int imq_set_credentials(imq_socket_t *socket, const char *username,
    const char *password);
int imq_set_getpw_func(imq_socket_t *socket, imq_getpw_cb getpw_cb);

int imq_bind_tcp(imq_socket_t *socket, const char *host, unsigned short port);
int imq_connect_tcp(imq_socket_t *socket, const char *host,
    unsigned short port);

int imq_bind_unix(imq_socket_t *socket, const char *path);
int imq_connect_unix(imq_socket_t *socket, const char *path);

void imq_attach(imq_socket_t *socket, const char *channel, void *zmqsocket);
void imq_detach(imq_socket_t *socket, void *zmqsocket);

void *imq_open_channel(imq_socket_t *socket, const char *channel, int type);
