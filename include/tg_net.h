/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_NET_H
#define TG_NET_H

/**
 * TCP/network operation result.
 *
 * TG_NET_CLOSED means the peer closed the connection cleanly. TG_NET_UNSUPPORTED
 * is returned by platform stubs that do not implement networking yet.
 * TG_NET_TIMEOUT means a bounded recv() elapsed with no data and nothing
 * consumed from the stream, so the caller may safely retry (used by the
 * encrypted-query receive loop to keep polling within its time budget instead
 * of hard-failing on the first quiet interval).
 */
typedef enum tg_net_status {
    TG_NET_OK = 0,
    TG_NET_INVALID_ARGUMENT = 1,
    TG_NET_RESOLVE_FAILED = 2,
    TG_NET_CONNECT_FAILED = 3,
    TG_NET_SEND_FAILED = 4,
    TG_NET_RECV_FAILED = 5,
    TG_NET_CLOSED = 6,
    TG_NET_UNSUPPORTED = 7,
    TG_NET_TIMEOUT = 8
} tg_net_status;

/**
 * Portable connection handle.
 *
 * platform_handle is owned by the platform backend while is_open is non-zero.
 * Callers should initialize with tg_net_connection_init() and close with
 * tg_net_close().
 */
typedef struct tg_net_connection {
    long platform_handle;
    int is_open;
} tg_net_connection;

/**
 * Resets a connection object to a closed state.
 */
void tg_net_connection_init(tg_net_connection *connection);

/**
 * Sets the TCP connect timeout used by platform backends that support it.
 *
 * A value of 0 keeps the platform default blocking behavior. Non-zero values
 * are expressed in seconds and are clamped by tg_net_set_connect_timeout_seconds().
 */
void tg_net_set_connect_timeout_seconds(unsigned long seconds);

/**
 * Returns the currently configured TCP connect timeout in seconds.
 *
 * A return value of 0 means no application-level timeout was requested.
 */
unsigned long tg_net_connect_timeout_seconds(void);

/**
 * Opens a TCP connection to host:port.
 *
 * error_buffer is caller-owned and optional; when provided, the platform may
 * write a NUL-terminated diagnostic string. The connection must be closed with
 * tg_net_close() after a successful return.
 */
tg_net_status tg_net_connect(tg_net_connection *connection, const char *host, const char *port,
                             char *error_buffer, unsigned long error_buffer_size);

/**
 * Sends up to byte_count bytes.
 *
 * bytes_sent is caller-owned and receives the actual number of bytes written.
 * Short writes are possible and should be handled by the caller.
 */
tg_net_status tg_net_send(tg_net_connection *connection, const void *data,
                          unsigned long byte_count, unsigned long *bytes_sent,
                          char *error_buffer, unsigned long error_buffer_size);

/**
 * Receives up to buffer_size bytes into caller-owned buffer.
 *
 * bytes_received receives the actual number of bytes read. TG_NET_CLOSED means
 * the remote peer closed the connection and no more data is available.
 */
tg_net_status tg_net_recv(tg_net_connection *connection, void *buffer,
                          unsigned long buffer_size, unsigned long *bytes_received,
                          char *error_buffer, unsigned long error_buffer_size);

/**
 * Tests whether an open TCP connection can be read without blocking.
 *
 * Returns 1 when data (or EOF) is ready, 0 when the socket is currently quiet,
 * and -1 on invalid arguments or a platform polling error.
 */
int tg_net_poll_readable(tg_net_connection *connection,
                         char *error_buffer, unsigned long error_buffer_size);

/**
 * Closes an open connection. Safe to call on an already closed object.
 */
void tg_net_close(tg_net_connection *connection);

/**
 * Convenience helper that connects and immediately closes the TCP connection.
 */
tg_net_status tg_net_tcp_probe(const char *host, const char *port,
                               char *error_buffer, unsigned long error_buffer_size);

/**
 * Returns a static string for status. The caller must not free it.
 */
const char *tg_net_status_name(tg_net_status status);

#endif
