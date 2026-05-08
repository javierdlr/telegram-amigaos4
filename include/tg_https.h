/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#ifndef TG_HTTPS_H
#define TG_HTTPS_H

#include "tg_net.h"
#include "tg_tls.h"

typedef enum tg_https_status {
    TG_HTTPS_OK = 0,
    TG_HTTPS_INVALID_ARGUMENT = 1,
    TG_HTTPS_REQUEST_TOO_LARGE = 2,
    TG_HTTPS_RESPONSE_TOO_LARGE = 3,
    TG_HTTPS_TLS_ERROR = 4
} tg_https_status;

tg_https_status tg_https_get(const char *host, const char *port, const char *path,
                             char *response_buffer, unsigned long response_buffer_size,
                             unsigned long *response_length, tg_tls_status *tls_status,
                             tg_net_status *net_status, char *error_buffer,
                             unsigned long error_buffer_size);
const char *tg_https_status_name(tg_https_status status);

#endif
