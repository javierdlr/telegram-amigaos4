/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_PLATFORM_H
#define TG_PLATFORM_H

#include "tg_net.h"
#include "tg_tls.h"

/**
 * Returns the human-readable platform name.
 *
 * The returned string is static and must not be freed.
 */
const char *tg_platform_name(void);

/**
 * Returns the default application data directory for the platform.
 *
 * The returned string is static and must not be freed.
 */
const char *tg_platform_default_data_dir(void);

/**
 * Emits one log message through the platform-specific logging backend.
 *
 * level and message are borrowed for the duration of the call.
 */
void tg_platform_log(const char *level, const char *message);

/**
 * Suspends execution for approximately the requested number of seconds.
 *
 * A value of zero returns immediately. This is used only by bounded polling
 * commands; platform backends may use the simplest native sleep primitive.
 */
void tg_platform_sleep_seconds(unsigned long seconds);

/**
 * Waits until standard input appears readable, or until timeout_seconds pass.
 *
 * Returns non-zero when a line can probably be read with fgets(), zero on
 * timeout or when the platform cannot test standard input readiness. A timeout
 * of zero performs a non-blocking poll.
 */
int tg_platform_stdin_readable(unsigned long timeout_seconds);

/**
 * Reads one character from standard input after waiting up to timeout_seconds.
 *
 * Returns 1 when one character was read, 0 on timeout and -1 when input is
 * closed or failed. Unlike tg_platform_stdin_readable()+fgets(), this lets
 * interactive loops keep polling while the user has not completed a line yet.
 */
int tg_platform_stdin_read_char(unsigned long timeout_seconds, char *out_char);

/**
 * Fills bytes with platform-provided random data.
 *
 * Returns non-zero on success. A zero return means the platform backend does
 * not currently expose a suitable source; callers that create persistent
 * secrets must fail closed in that case.
 */
int tg_platform_random_bytes(unsigned char *bytes, unsigned long byte_count);

/**
 * Platform TCP connect implementation used by tg_net_connect().
 *
 * On success, connection must contain a valid platform_handle and is_open must
 * be non-zero. error_buffer is caller-owned and optional.
 */
tg_net_status tg_platform_tcp_connect(tg_net_connection *connection, const char *host,
                                      const char *port, char *error_buffer,
                                      unsigned long error_buffer_size);

/**
 * Platform TCP send implementation used by tg_net_send().
 */
tg_net_status tg_platform_tcp_send(tg_net_connection *connection, const void *data,
                                   unsigned long byte_count, unsigned long *bytes_sent,
                                   char *error_buffer, unsigned long error_buffer_size);

/**
 * Platform TCP receive implementation used by tg_net_recv().
 */
tg_net_status tg_platform_tcp_recv(tg_net_connection *connection, void *buffer,
                                   unsigned long buffer_size, unsigned long *bytes_received,
                                   char *error_buffer, unsigned long error_buffer_size);

/**
 * Platform TCP close implementation used by tg_net_close().
 */
void tg_platform_tcp_close(tg_net_connection *connection);

/**
 * Platform TLS connect implementation used by tg_tls_connect().
 *
 * Backends should set net_status when the failure originates in the TCP layer.
 */
tg_tls_status tg_platform_tls_connect(tg_tls_connection *connection, const char *host,
                                      const char *port, tg_net_status *net_status,
                                      char *error_buffer, unsigned long error_buffer_size);

/**
 * Platform TLS send implementation used by tg_tls_send().
 */
tg_tls_status tg_platform_tls_send(tg_tls_connection *connection, const void *data,
                                   unsigned long byte_count, unsigned long *bytes_sent,
                                   char *error_buffer, unsigned long error_buffer_size);

/**
 * Platform TLS receive implementation used by tg_tls_recv().
 */
tg_tls_status tg_platform_tls_recv(tg_tls_connection *connection, void *buffer,
                                   unsigned long buffer_size, unsigned long *bytes_received,
                                   char *error_buffer, unsigned long error_buffer_size);

/**
 * Platform TLS close implementation used by tg_tls_close().
 */
void tg_platform_tls_close(tg_tls_connection *connection);

#endif
