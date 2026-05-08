/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include "tg_net.h"
#include "tg_platform.h"

tg_net_status tg_net_tcp_probe(const char *host, const char *port,
                               char *error_buffer, unsigned long error_buffer_size)
{
    if (host == 0 || port == 0 || host[0] == '\0' || port[0] == '\0') {
        return TG_NET_INVALID_ARGUMENT;
    }

    return tg_platform_tcp_probe(host, port, error_buffer, error_buffer_size);
}

const char *tg_net_status_name(tg_net_status status)
{
    switch (status) {
    case TG_NET_OK:
        return "ok";
    case TG_NET_INVALID_ARGUMENT:
        return "invalid-argument";
    case TG_NET_RESOLVE_FAILED:
        return "resolve-failed";
    case TG_NET_CONNECT_FAILED:
        return "connect-failed";
    case TG_NET_UNSUPPORTED:
        return "unsupported";
    default:
        return "unknown";
    }
}
