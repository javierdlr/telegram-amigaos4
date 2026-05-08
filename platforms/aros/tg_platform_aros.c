/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <stdio.h>

#include "tg_platform.h"

const char *tg_platform_name(void)
{
    return "AROS";
}

const char *tg_platform_default_data_dir(void)
{
    return "PROGDIR:";
}

void tg_platform_log(const char *level, const char *message)
{
    printf("[aros:%s] %s\n", level, message);
}

tg_net_status tg_platform_tcp_probe(const char *host, const char *port,
                                    char *error_buffer, unsigned long error_buffer_size)
{
    (void)host;
    (void)port;
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    return TG_NET_UNSUPPORTED;
}
