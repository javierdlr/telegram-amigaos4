/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_HTTP_H
#define TG_HTTP_H

#include "tg_net.h"

typedef enum tg_http_status {
    TG_HTTP_OK = 0,
    TG_HTTP_INVALID_ARGUMENT = 1,
    TG_HTTP_REQUEST_TOO_LARGE = 2,
    TG_HTTP_RESPONSE_TOO_LARGE = 3,
    TG_HTTP_NET_ERROR = 4
} tg_http_status;

typedef enum tg_http_parse_status {
    TG_HTTP_PARSE_OK = 0,
    TG_HTTP_PARSE_INVALID_ARGUMENT = 1,
    TG_HTTP_PARSE_INCOMPLETE = 2,
    TG_HTTP_PARSE_INVALID_RESPONSE = 3
} tg_http_parse_status;

typedef struct tg_http_response {
    int status_code;
    const char *reason;
    unsigned long reason_length;
    const char *headers;
    unsigned long headers_length;
    const char *body;
    unsigned long body_length;
} tg_http_response;

tg_http_status tg_http_get(const char *host, const char *port, const char *path,
                           char *response_buffer, unsigned long response_buffer_size,
                           unsigned long *response_length, tg_net_status *net_status,
                           char *error_buffer, unsigned long error_buffer_size);
tg_http_parse_status tg_http_parse_response(const char *response, unsigned long response_length,
                                            tg_http_response *parsed_response);
const char *tg_http_status_name(tg_http_status status);
const char *tg_http_parse_status_name(tg_http_parse_status status);

#endif
