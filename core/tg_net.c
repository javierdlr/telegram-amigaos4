/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include "tg_net.h"
#include "tg_platform.h"

static unsigned long tg_connect_timeout_seconds = 0;

void tg_net_connection_init(tg_net_connection *connection)
{
    if (connection != 0) {
        connection->platform_handle = -1;
        connection->is_open = 0;
    }
}

void tg_net_set_connect_timeout_seconds(unsigned long seconds)
{
    if (seconds > 3600UL) {
        seconds = 3600UL;
    }
    tg_connect_timeout_seconds = seconds;
}

unsigned long tg_net_connect_timeout_seconds(void)
{
    return tg_connect_timeout_seconds;
}

tg_net_status tg_net_connect(tg_net_connection *connection, const char *host, const char *port,
                             char *error_buffer, unsigned long error_buffer_size)
{
    if (connection == 0 || host == 0 || port == 0 || host[0] == '\0' || port[0] == '\0') {
        return TG_NET_INVALID_ARGUMENT;
    }

    tg_net_connection_init(connection);
    return tg_platform_tcp_connect(connection, host, port, error_buffer, error_buffer_size);
}

tg_net_status tg_net_send(tg_net_connection *connection, const void *data,
                          unsigned long byte_count, unsigned long *bytes_sent,
                          char *error_buffer, unsigned long error_buffer_size)
{
    if (bytes_sent != 0) {
        *bytes_sent = 0;
    }
    if (connection == 0 || data == 0 || byte_count == 0) {
        return TG_NET_INVALID_ARGUMENT;
    }
    if (!connection->is_open) {
        return TG_NET_CLOSED;
    }

    return tg_platform_tcp_send(connection, data, byte_count, bytes_sent,
                                error_buffer, error_buffer_size);
}

tg_net_status tg_net_recv(tg_net_connection *connection, void *buffer,
                          unsigned long buffer_size, unsigned long *bytes_received,
                          char *error_buffer, unsigned long error_buffer_size)
{
    if (bytes_received != 0) {
        *bytes_received = 0;
    }
    if (connection == 0 || buffer == 0 || buffer_size == 0) {
        return TG_NET_INVALID_ARGUMENT;
    }
    if (!connection->is_open) {
        return TG_NET_CLOSED;
    }

    return tg_platform_tcp_recv(connection, buffer, buffer_size, bytes_received,
                                error_buffer, error_buffer_size);
}

int tg_net_poll_readable(tg_net_connection *connection,
                         char *error_buffer, unsigned long error_buffer_size)
{
    if (error_buffer != 0 && error_buffer_size > 0UL) {
        error_buffer[0] = '\0';
    }
    if (connection == 0 || !connection->is_open) {
        return -1;
    }
    return tg_platform_tcp_poll_readable(connection, error_buffer,
                                         error_buffer_size);
}

void tg_net_close(tg_net_connection *connection)
{
    if (connection != 0 && connection->is_open) {
        tg_platform_tcp_close(connection);
    }
    tg_net_connection_init(connection);
}

tg_net_status tg_net_tcp_probe(const char *host, const char *port,
                               char *error_buffer, unsigned long error_buffer_size)
{
    tg_net_connection connection;
    tg_net_status status;

    if (host == 0 || port == 0 || host[0] == '\0' || port[0] == '\0') {
        return TG_NET_INVALID_ARGUMENT;
    }

    status = tg_net_connect(&connection, host, port, error_buffer, error_buffer_size);
    if (status == TG_NET_OK) {
        tg_net_close(&connection);
    }
    return status;
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
    case TG_NET_SEND_FAILED:
        return "send-failed";
    case TG_NET_RECV_FAILED:
        return "recv-failed";
    case TG_NET_CLOSED:
        return "closed";
    case TG_NET_UNSUPPORTED:
        return "unsupported";
    case TG_NET_TIMEOUT:
        return "timeout";
    default:
        return "unknown";
    }
}
