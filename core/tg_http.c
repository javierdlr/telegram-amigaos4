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

static int tg_http_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int tg_http_find_line_end(const char *data, unsigned long data_length,
                                 unsigned long start, unsigned long *line_end,
                                 unsigned long *next_line)
{
    unsigned long i;

    i = start;
    while (i < data_length) {
        if (data[i] == '\n') {
            if (i > start && data[i - 1] == '\r') {
                *line_end = i - 1;
            } else {
                *line_end = i;
            }
            *next_line = i + 1;
            return 1;
        }
        ++i;
    }

    return 0;
}

static int tg_http_find_header_end(const char *data, unsigned long data_length,
                                   unsigned long *header_end, unsigned long *body_start)
{
    unsigned long i;

    i = 0;
    while (i + 3 < data_length) {
        if (data[i] == '\r' && data[i + 1] == '\n' &&
            data[i + 2] == '\r' && data[i + 3] == '\n') {
            *header_end = i;
            *body_start = i + 4;
            return 1;
        }
        ++i;
    }

    i = 0;
    while (i + 1 < data_length) {
        if (data[i] == '\n' && data[i + 1] == '\n') {
            *header_end = i;
            *body_start = i + 2;
            return 1;
        }
        ++i;
    }

    return 0;
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

tg_http_parse_status tg_http_parse_response(const char *response, unsigned long response_length,
                                            tg_http_response *parsed_response)
{
    unsigned long line_end;
    unsigned long next_line;
    unsigned long header_end;
    unsigned long body_start;
    unsigned long status_pos;
    unsigned long reason_start;
    int status_code;

    if (parsed_response != 0) {
        parsed_response->status_code = 0;
        parsed_response->reason = 0;
        parsed_response->reason_length = 0;
        parsed_response->headers = 0;
        parsed_response->headers_length = 0;
        parsed_response->body = 0;
        parsed_response->body_length = 0;
    }

    if (response == 0 || parsed_response == 0) {
        return TG_HTTP_PARSE_INVALID_ARGUMENT;
    }

    if (!tg_http_find_header_end(response, response_length, &header_end, &body_start)) {
        return TG_HTTP_PARSE_INCOMPLETE;
    }
    if (!tg_http_find_line_end(response, response_length, 0, &line_end, &next_line)) {
        return TG_HTTP_PARSE_INCOMPLETE;
    }

    if (line_end < 12 || strncmp(response, "HTTP/", 5) != 0) {
        return TG_HTTP_PARSE_INVALID_RESPONSE;
    }

    status_pos = 5;
    while (status_pos < line_end && response[status_pos] != ' ') {
        ++status_pos;
    }
    while (status_pos < line_end && response[status_pos] == ' ') {
        ++status_pos;
    }

    if (status_pos + 2 >= line_end ||
        !tg_http_is_digit(response[status_pos]) ||
        !tg_http_is_digit(response[status_pos + 1]) ||
        !tg_http_is_digit(response[status_pos + 2])) {
        return TG_HTTP_PARSE_INVALID_RESPONSE;
    }

    status_code = (response[status_pos] - '0') * 100;
    status_code += (response[status_pos + 1] - '0') * 10;
    status_code += response[status_pos + 2] - '0';

    reason_start = status_pos + 3;
    while (reason_start < line_end && response[reason_start] == ' ') {
        ++reason_start;
    }

    parsed_response->status_code = status_code;
    parsed_response->reason = response + reason_start;
    parsed_response->reason_length = line_end - reason_start;
    parsed_response->headers = response + next_line;
    parsed_response->headers_length = header_end - next_line;
    parsed_response->body = response + body_start;
    parsed_response->body_length = response_length - body_start;

    return TG_HTTP_PARSE_OK;
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

const char *tg_http_parse_status_name(tg_http_parse_status status)
{
    switch (status) {
    case TG_HTTP_PARSE_OK:
        return "ok";
    case TG_HTTP_PARSE_INVALID_ARGUMENT:
        return "invalid-argument";
    case TG_HTTP_PARSE_INCOMPLETE:
        return "incomplete";
    case TG_HTTP_PARSE_INVALID_RESPONSE:
        return "invalid-response";
    default:
        return "unknown";
    }
}
