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
    TG_NET_UNSUPPORTED = 4
} tg_net_status;

tg_net_status tg_net_tcp_probe(const char *host, const char *port,
                               char *error_buffer, unsigned long error_buffer_size);
const char *tg_net_status_name(tg_net_status status);

#endif
