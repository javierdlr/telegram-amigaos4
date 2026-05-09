/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__amigaos3__)
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "tg_platform.h"

const char *tg_platform_name(void)
{
    return "AmigaOS 3.x";
}

const char *tg_platform_default_data_dir(void)
{
    return "PROGDIR:";
}

void tg_platform_log(const char *level, const char *message)
{
    printf("[amigaos3:%s] %s\n", level, message);
}

void tg_platform_sleep_seconds(unsigned long seconds)
{
#if defined(__amigaos3__)
    if (seconds > 0) {
        sleep(seconds);
    }
#else
    time_t start;

    if (seconds == 0) {
        return;
    }

    start = time(0);
    if (start == (time_t)-1) {
        return;
    }
    while ((unsigned long)(time(0) - start) < seconds) {
    }
#endif
}

#if defined(__amigaos3__)

static void tg_platform_set_error(char *error_buffer, unsigned long error_buffer_size,
                                  const char *message)
{
    if (error_buffer != 0 && error_buffer_size > 0) {
        strncpy(error_buffer, message, error_buffer_size - 1);
        error_buffer[error_buffer_size - 1] = '\0';
    }
}

tg_net_status tg_platform_tcp_connect(tg_net_connection *connection, const char *host,
                                      const char *port, char *error_buffer,
                                      unsigned long error_buffer_size)
{
    struct hostent *host_entry;
    struct sockaddr_in address;
    long port_number;
    int sock;
    int rc;

    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    port_number = strtol(port, 0, 10);
    if (port_number <= 0 || port_number > 65535) {
        return TG_NET_INVALID_ARGUMENT;
    }

    host_entry = gethostbyname((char *)host);
    if (host_entry == 0 || host_entry->h_addr_list == 0 ||
        host_entry->h_addr_list[0] == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, "host lookup failed");
        return TG_NET_RESOLVE_FAILED;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port_number);
    memcpy(&address.sin_addr, host_entry->h_addr_list[0], sizeof(address.sin_addr));

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, "socket open failed");
        return TG_NET_CONNECT_FAILED;
    }

    rc = connect(sock, (struct sockaddr *)&address, sizeof(address));
    if (rc == 0) {
        connection->platform_handle = sock;
        connection->is_open = 1;
        return TG_NET_OK;
    }

    close(sock);
    tg_platform_set_error(error_buffer, error_buffer_size, "socket connect failed");
    return TG_NET_CONNECT_FAILED;
}

tg_net_status tg_platform_tcp_send(tg_net_connection *connection, const void *data,
                                   unsigned long byte_count, unsigned long *bytes_sent,
                                   char *error_buffer, unsigned long error_buffer_size)
{
    long rc;

    if (bytes_sent != 0) {
        *bytes_sent = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    rc = send(connection->platform_handle, (void *)data, (long)byte_count, 0);
    if (rc < 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, "socket send failed");
        return TG_NET_SEND_FAILED;
    }
    if (bytes_sent != 0) {
        *bytes_sent = (unsigned long)rc;
    }
    return TG_NET_OK;
}

tg_net_status tg_platform_tcp_recv(tg_net_connection *connection, void *buffer,
                                   unsigned long buffer_size, unsigned long *bytes_received,
                                   char *error_buffer, unsigned long error_buffer_size)
{
    long rc;

    if (bytes_received != 0) {
        *bytes_received = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    rc = recv(connection->platform_handle, buffer, (long)buffer_size, 0);
    if (rc < 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, "socket receive failed");
        return TG_NET_RECV_FAILED;
    }
    if (rc == 0) {
        return TG_NET_CLOSED;
    }
    if (bytes_received != 0) {
        *bytes_received = (unsigned long)rc;
    }
    return TG_NET_OK;
}

void tg_platform_tcp_close(tg_net_connection *connection)
{
    if (connection != 0 && connection->is_open) {
        close((int)connection->platform_handle);
    }
}

#else

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

#endif

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
