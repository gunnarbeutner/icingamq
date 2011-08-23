#ifndef LIBIMQ_H
#define LIBIMQ_H

typedef int (*imq_authn_getpw_cb)(const char *username, char **password);
typedef int (*imq_authz_channel_cb)(const char *username, const char *channel);

typedef struct imq_socket_s imq_socket_t;
typedef struct imq_listener_s imq_listener_t;

imq_socket_t *imq_connect(const char *host, unsigned short port,
    const char *username, const char *password);
void *imq_open_channel(imq_socket_t *socket, const char *channel,
    const char *instance, int zmqtype);
void imq_close(imq_socket_t *socket);

imq_listener_t *imq_listener(unsigned short port, imq_authn_getpw_cb getpw_cb,
    imq_authz_channel_cb authz_channel_cb);
void *imq_create_channel(imq_listener_t *listener, const char *channel,
    const char *instance, int zmqtype);

int imq_device(imq_listener_t *downstream, imq_socket_t *upstream,
    int zmqdevice);

#endif /* LIBIMQ_H */
