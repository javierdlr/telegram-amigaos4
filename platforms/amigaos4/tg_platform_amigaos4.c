/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "tg_platform.h"

const char *tg_platform_name(void)
{
    return "AmigaOS 4.x";
}

const char *tg_platform_default_data_dir(void)
{
    return "PROGDIR:";
}

void tg_platform_log(const char *level, const char *message)
{
    printf("[amigaos4:%s] %s\n", level, message);
}

tg_net_status tg_platform_tcp_connect(tg_net_connection *connection, const char *host,
                                      const char *port, char *error_buffer,
                                      unsigned long error_buffer_size)
{
    (void)connection;
    (void)host;
    (void)port;
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    return TG_NET_UNSUPPORTED;
}

tg_net_status tg_platform_tcp_send(tg_net_connection *connection, const void *data,
                                   unsigned long byte_count, unsigned long *bytes_sent,
                                   char *error_buffer, unsigned long error_buffer_size)
{
    (void)connection;
    (void)data;
    (void)byte_count;
    if (bytes_sent != 0) {
        *bytes_sent = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    return TG_NET_UNSUPPORTED;
}

tg_net_status tg_platform_tcp_recv(tg_net_connection *connection, void *buffer,
                                   unsigned long buffer_size, unsigned long *bytes_received,
                                   char *error_buffer, unsigned long error_buffer_size)
{
    (void)connection;
    (void)buffer;
    (void)buffer_size;
    if (bytes_received != 0) {
        *bytes_received = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    return TG_NET_UNSUPPORTED;
}

void tg_platform_tcp_close(tg_net_connection *connection)
{
    (void)connection;
}

tg_tls_status tg_platform_tls_connect(tg_tls_connection *connection, const char *host,
                                      const char *port, tg_net_status *net_status,
                                      char *error_buffer, unsigned long error_buffer_size)
{
    (void)connection;
    (void)host;
    (void)port;
    if (net_status != 0) {
        *net_status = TG_NET_OK;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    return TG_TLS_UNSUPPORTED;
}

tg_tls_status tg_platform_tls_send(tg_tls_connection *connection, const void *data,
                                   unsigned long byte_count, unsigned long *bytes_sent,
                                   char *error_buffer, unsigned long error_buffer_size)
{
    (void)connection;
    (void)data;
    (void)byte_count;
    if (bytes_sent != 0) {
        *bytes_sent = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    return TG_TLS_UNSUPPORTED;
}

tg_tls_status tg_platform_tls_recv(tg_tls_connection *connection, void *buffer,
                                   unsigned long buffer_size, unsigned long *bytes_received,
                                   char *error_buffer, unsigned long error_buffer_size)
{
    (void)connection;
    (void)buffer;
    (void)buffer_size;
    if (bytes_received != 0) {
        *bytes_received = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    return TG_TLS_UNSUPPORTED;
}

void tg_platform_tls_close(tg_tls_connection *connection)
{
    (void)connection;
}
