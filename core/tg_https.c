/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <string.h>

#include "tg_https.h"

static tg_https_status tg_https_build_get_request(const char *host, const char *path,
                                                  char *request, unsigned long request_size,
                                                  unsigned long *request_length)
{
    unsigned long needed;

    needed = (unsigned long)strlen(host) + (unsigned long)strlen(path) + 43;
    if (needed >= request_size) {
        return TG_HTTPS_REQUEST_TOO_LARGE;
    }

    strcpy(request, "GET ");
    strcat(request, path);
    strcat(request, " HTTP/1.0\r\nHost: ");
    strcat(request, host);
    strcat(request, "\r\nConnection: close\r\n\r\n");
    *request_length = (unsigned long)strlen(request);
    return TG_HTTPS_OK;
}

tg_https_status tg_https_get(const char *host, const char *port, const char *path,
                             char *response_buffer, unsigned long response_buffer_size,
                             unsigned long *response_length, tg_tls_status *tls_status,
                             tg_net_status *net_status, char *error_buffer,
                             unsigned long error_buffer_size)
{
    tg_tls_connection connection;
    tg_https_status https_status;
    tg_tls_status local_tls_status;
    char request[512];
    unsigned long request_length;
    unsigned long total_sent;
    unsigned long bytes_done;
    unsigned long total_received;

    if (response_length != 0) {
        *response_length = 0;
    }
    if (tls_status != 0) {
        *tls_status = TG_TLS_OK;
    }
    if (net_status != 0) {
        *net_status = TG_NET_OK;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    if (host == 0 || port == 0 || path == 0 || response_buffer == 0 ||
        response_buffer_size < 2 || response_length == 0) {
        return TG_HTTPS_INVALID_ARGUMENT;
    }

    https_status = tg_https_build_get_request(host, path, request, sizeof(request), &request_length);
    if (https_status != TG_HTTPS_OK) {
        return https_status;
    }

    local_tls_status = tg_tls_connect(&connection, host, port, net_status,
                                      error_buffer, error_buffer_size);
    if (local_tls_status != TG_TLS_OK) {
        if (tls_status != 0) {
            *tls_status = local_tls_status;
        }
        return TG_HTTPS_TLS_ERROR;
    }

    total_sent = 0;
    while (total_sent < request_length) {
        local_tls_status = tg_tls_send(&connection, request + total_sent,
                                       request_length - total_sent, &bytes_done,
                                       error_buffer, error_buffer_size);
        if (local_tls_status != TG_TLS_OK || bytes_done == 0) {
            tg_tls_close(&connection);
            if (tls_status != 0) {
                *tls_status = local_tls_status;
            }
            return TG_HTTPS_TLS_ERROR;
        }
        total_sent += bytes_done;
    }

    total_received = 0;
    while (total_received < response_buffer_size - 1) {
        local_tls_status = tg_tls_recv(&connection, response_buffer + total_received,
                                       response_buffer_size - 1 - total_received, &bytes_done,
                                       error_buffer, error_buffer_size);
        if (local_tls_status == TG_TLS_CLOSED) {
            break;
        }
        if (local_tls_status != TG_TLS_OK) {
            tg_tls_close(&connection);
            response_buffer[total_received] = '\0';
            *response_length = total_received;
            if (tls_status != 0) {
                *tls_status = local_tls_status;
            }
            return TG_HTTPS_TLS_ERROR;
        }
        if (bytes_done == 0) {
            break;
        }
        total_received += bytes_done;
    }

    tg_tls_close(&connection);
    response_buffer[total_received] = '\0';
    *response_length = total_received;

    if (total_received >= response_buffer_size - 1) {
        return TG_HTTPS_RESPONSE_TOO_LARGE;
    }

    return TG_HTTPS_OK;
}

const char *tg_https_status_name(tg_https_status status)
{
    switch (status) {
    case TG_HTTPS_OK:
        return "ok";
    case TG_HTTPS_INVALID_ARGUMENT:
        return "invalid-argument";
    case TG_HTTPS_REQUEST_TOO_LARGE:
        return "request-too-large";
    case TG_HTTPS_RESPONSE_TOO_LARGE:
        return "response-too-large";
    case TG_HTTPS_TLS_ERROR:
        return "tls-error";
    default:
        return "unknown";
    }
}
