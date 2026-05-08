/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#ifndef TG_NET_H
#define TG_NET_H

typedef enum tg_net_status {
    TG_NET_OK = 0,
    TG_NET_INVALID_ARGUMENT = 1,
    TG_NET_RESOLVE_FAILED = 2,
    TG_NET_CONNECT_FAILED = 3,
    TG_NET_SEND_FAILED = 4,
    TG_NET_RECV_FAILED = 5,
    TG_NET_CLOSED = 6,
    TG_NET_UNSUPPORTED = 7
} tg_net_status;

typedef struct tg_net_connection {
    long platform_handle;
    int is_open;
} tg_net_connection;

void tg_net_connection_init(tg_net_connection *connection);
tg_net_status tg_net_connect(tg_net_connection *connection, const char *host, const char *port,
                             char *error_buffer, unsigned long error_buffer_size);
tg_net_status tg_net_send(tg_net_connection *connection, const void *data,
                          unsigned long byte_count, unsigned long *bytes_sent,
                          char *error_buffer, unsigned long error_buffer_size);
tg_net_status tg_net_recv(tg_net_connection *connection, void *buffer,
                          unsigned long buffer_size, unsigned long *bytes_received,
                          char *error_buffer, unsigned long error_buffer_size);
void tg_net_close(tg_net_connection *connection);
tg_net_status tg_net_tcp_probe(const char *host, const char *port,
                               char *error_buffer, unsigned long error_buffer_size);
const char *tg_net_status_name(tg_net_status status);

#endif
