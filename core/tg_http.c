/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <string.h>

#include "tg_http.h"

static void tg_http_clear_error(char *error_buffer, unsigned long error_buffer_size)
{
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
}

static tg_http_status tg_http_build_get_request(const char *host, const char *path,
                                                char *request, unsigned long request_size,
                                                unsigned long *request_length)
{
    unsigned long needed;

    needed = (unsigned long)strlen(host) + (unsigned long)strlen(path) + 43;
    if (needed >= request_size) {
        return TG_HTTP_REQUEST_TOO_LARGE;
    }

    strcpy(request, "GET ");
    strcat(request, path);
    strcat(request, " HTTP/1.0\r\nHost: ");
    strcat(request, host);
    strcat(request, "\r\nConnection: close\r\n\r\n");
    *request_length = (unsigned long)strlen(request);
    return TG_HTTP_OK;
}

tg_http_status tg_http_get(const char *host, const char *port, const char *path,
                           char *response_buffer, unsigned long response_buffer_size,
                           unsigned long *response_length, tg_net_status *net_status,
                           char *error_buffer, unsigned long error_buffer_size)
{
    tg_net_connection connection;
    tg_http_status http_status;
    tg_net_status local_net_status;
    char request[512];
    unsigned long request_length;
    unsigned long total_sent;
    unsigned long bytes_done;
    unsigned long total_received;

    if (response_length != 0) {
        *response_length = 0;
    }
    if (net_status != 0) {
        *net_status = TG_NET_OK;
    }
    tg_http_clear_error(error_buffer, error_buffer_size);

    if (host == 0 || port == 0 || path == 0 || response_buffer == 0 ||
        response_buffer_size < 2 || response_length == 0) {
        return TG_HTTP_INVALID_ARGUMENT;
    }

    http_status = tg_http_build_get_request(host, path, request, sizeof(request), &request_length);
    if (http_status != TG_HTTP_OK) {
        return http_status;
    }

    local_net_status = tg_net_connect(&connection, host, port, error_buffer, error_buffer_size);
    if (local_net_status != TG_NET_OK) {
        if (net_status != 0) {
            *net_status = local_net_status;
        }
        return TG_HTTP_NET_ERROR;
    }

    total_sent = 0;
    while (total_sent < request_length) {
        local_net_status = tg_net_send(&connection, request + total_sent,
                                       request_length - total_sent, &bytes_done,
                                       error_buffer, error_buffer_size);
        if (local_net_status != TG_NET_OK) {
            tg_net_close(&connection);
            if (net_status != 0) {
                *net_status = local_net_status;
            }
            return TG_HTTP_NET_ERROR;
        }
        if (bytes_done == 0) {
            tg_net_close(&connection);
            if (net_status != 0) {
                *net_status = TG_NET_SEND_FAILED;
            }
            return TG_HTTP_NET_ERROR;
        }
        total_sent += bytes_done;
    }

    total_received = 0;
    while (total_received < response_buffer_size - 1) {
        local_net_status = tg_net_recv(&connection, response_buffer + total_received,
                                       response_buffer_size - 1 - total_received, &bytes_done,
                                       error_buffer, error_buffer_size);
        if (local_net_status == TG_NET_CLOSED) {
            break;
        }
        if (local_net_status != TG_NET_OK) {
            tg_net_close(&connection);
            response_buffer[total_received] = '\0';
            *response_length = total_received;
            if (net_status != 0) {
                *net_status = local_net_status;
            }
            return TG_HTTP_NET_ERROR;
        }
        if (bytes_done == 0) {
            break;
        }
        total_received += bytes_done;
    }

    tg_net_close(&connection);
    response_buffer[total_received] = '\0';
    *response_length = total_received;

    if (total_received >= response_buffer_size - 1) {
        return TG_HTTP_RESPONSE_TOO_LARGE;
    }

    return TG_HTTP_OK;
}

const char *tg_http_status_name(tg_http_status status)
{
    switch (status) {
    case TG_HTTP_OK:
        return "ok";
    case TG_HTTP_INVALID_ARGUMENT:
        return "invalid-argument";
    case TG_HTTP_REQUEST_TOO_LARGE:
        return "request-too-large";
    case TG_HTTP_RESPONSE_TOO_LARGE:
        return "response-too-large";
    case TG_HTTP_NET_ERROR:
        return "net-error";
    default:
        return "unknown";
    }
}
