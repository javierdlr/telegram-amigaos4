/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_HTTP_H
#define TG_HTTP_H

#include "tg_net.h"

/**
 * HTTP transfer result.
 *
 * TG_HTTP_RESPONSE_TOO_LARGE means response_buffer filled before the complete
 * response was received. TG_HTTP_NET_ERROR means net_status contains the TCP
 * reason.
 */
typedef enum tg_http_status {
    TG_HTTP_OK = 0,
    TG_HTTP_INVALID_ARGUMENT = 1,
    TG_HTTP_REQUEST_TOO_LARGE = 2,
    TG_HTTP_RESPONSE_TOO_LARGE = 3,
    TG_HTTP_NET_ERROR = 4
} tg_http_status;

/**
 * HTTP response parsing result.
 *
 * INCOMPLETE means no complete header block was found in the supplied bytes.
 * INVALID_RESPONSE means a header block exists but is not a valid HTTP response.
 */
typedef enum tg_http_parse_status {
    TG_HTTP_PARSE_OK = 0,
    TG_HTTP_PARSE_INVALID_ARGUMENT = 1,
    TG_HTTP_PARSE_INCOMPLETE = 2,
    TG_HTTP_PARSE_INVALID_RESPONSE = 3
} tg_http_parse_status;

/**
 * Parsed view of an HTTP response.
 *
 * All pointers reference the caller-owned response buffer passed to
 * tg_http_parse_response(); no memory is allocated and no data is copied.
 */
typedef struct tg_http_response {
    int status_code;
    const char *reason;
    unsigned long reason_length;
    const char *headers;
    unsigned long headers_length;
    const char *body;
    unsigned long body_length;
} tg_http_response;

/**
 * Performs a minimal HTTP/1.0 GET over TCP.
 *
 * response_buffer is caller-owned and receives a NUL-terminated full HTTP
 * response, including status line, headers and body. response_length receives
 * the number of response bytes excluding the final NUL. If response_buffer_size
 * is not large enough, TG_HTTP_RESPONSE_TOO_LARGE is returned.
 */
tg_http_status tg_http_get(const char *host, const char *port, const char *path,
                           char *response_buffer, unsigned long response_buffer_size,
                           unsigned long *response_length, tg_net_status *net_status,
                           char *error_buffer, unsigned long error_buffer_size);

/**
 * Parses a complete HTTP response already stored in memory.
 *
 * parsed_response contains borrowed pointers into response. The caller must keep
 * response alive and unchanged while using parsed_response.
 */
tg_http_parse_status tg_http_parse_response(const char *response, unsigned long response_length,
                                            tg_http_response *parsed_response);

/**
 * Returns a static string for status. The caller must not free it.
 */
const char *tg_http_status_name(tg_http_status status);

/**
 * Returns a static string for parse status. The caller must not free it.
 */
const char *tg_http_parse_status_name(tg_http_parse_status status);

#endif
