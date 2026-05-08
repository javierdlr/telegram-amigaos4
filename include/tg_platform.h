/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#ifndef TG_PLATFORM_H
#define TG_PLATFORM_H

#include "tg_net.h"

const char *tg_platform_name(void);
const char *tg_platform_default_data_dir(void);
void tg_platform_log(const char *level, const char *message);
tg_net_status tg_platform_tcp_probe(const char *host, const char *port,
                                    char *error_buffer, unsigned long error_buffer_size);

#endif
