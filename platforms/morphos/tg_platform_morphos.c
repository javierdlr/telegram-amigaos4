/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <proto/dos.h>

#ifndef TG_ENABLE_TLS
#define TG_ENABLE_TLS 0
#endif

#if TG_ENABLE_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#include "tg_platform.h"

const char *tg_platform_name(void)
{
    return "MorphOS";
}

const char *tg_platform_default_data_dir(void)
{
    return "PROGDIR:";
}

void tg_platform_log(const char *level, const char *message)
{
    printf("[morphos:%s] %s\n", level, message);
}

void tg_platform_sleep_seconds(unsigned long seconds)
{
    if (seconds > 0) {
        sleep(seconds);
    }
}

int tg_platform_stdin_readable(unsigned long timeout_seconds)
{
    unsigned long long timeout_microseconds;

    timeout_microseconds = (unsigned long long)timeout_seconds * 1000000ULL;
    if (timeout_microseconds > 2147000000ULL) {
        timeout_microseconds = 2147000000ULL;
    }
    return WaitForChar(Input(), (long)timeout_microseconds) != 0;
}

static void tg_platform_set_error(char *error_buffer, unsigned long error_buffer_size,
                                  const char *message)
{
    if (error_buffer != 0 && error_buffer_size > 0) {
        strncpy(error_buffer, message, error_buffer_size - 1);
        error_buffer[error_buffer_size - 1] = '\0';
    }
}

static tg_net_status tg_platform_connect_socket(int sock, struct sockaddr_in *address,
                                                char *error_buffer,
                                                unsigned long error_buffer_size)
{
    unsigned long timeout_seconds;
    int flags;
    int rc;
    int socket_error;
    long socket_error_size;
    fd_set write_fds;
    struct timeval timeout;

    timeout_seconds = tg_net_connect_timeout_seconds();
    if (timeout_seconds == 0) {
        rc = connect(sock, (struct sockaddr *)address, sizeof(*address));
        if (rc == 0) {
            return TG_NET_OK;
        }
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
        return TG_NET_CONNECT_FAILED;
    }

    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        rc = connect(sock, (struct sockaddr *)address, sizeof(*address));
        if (rc == 0) {
            return TG_NET_OK;
        }
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
        return TG_NET_CONNECT_FAILED;
    }

    rc = connect(sock, (struct sockaddr *)address, sizeof(*address));
    if (rc == 0) {
        (void)fcntl(sock, F_SETFL, flags);
        return TG_NET_OK;
    }
    if (errno != EINPROGRESS && errno != EWOULDBLOCK) {
        (void)fcntl(sock, F_SETFL, flags);
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
        return TG_NET_CONNECT_FAILED;
    }

    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);
    timeout.tv_sec = (long)timeout_seconds;
    timeout.tv_usec = 0;

    rc = select(sock + 1, 0, &write_fds, 0, &timeout);
    if (rc <= 0) {
        (void)fcntl(sock, F_SETFL, flags);
        if (rc == 0) {
            tg_platform_set_error(error_buffer, error_buffer_size,
                                  "socket connect timed out");
        } else {
            tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
        }
        return TG_NET_CONNECT_FAILED;
    }

    socket_error = 0;
    socket_error_size = sizeof(socket_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &socket_error,
                   &socket_error_size) != 0) {
        (void)fcntl(sock, F_SETFL, flags);
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
        return TG_NET_CONNECT_FAILED;
    }
    if (socket_error != 0) {
        (void)fcntl(sock, F_SETFL, flags);
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(socket_error));
        return TG_NET_CONNECT_FAILED;
    }

    (void)fcntl(sock, F_SETFL, flags);
    return TG_NET_OK;
}

tg_net_status tg_platform_tcp_connect(tg_net_connection *connection, const char *host,
                                      const char *port, char *error_buffer,
                                      unsigned long error_buffer_size)
{
    struct hostent *host_entry;
    struct sockaddr_in address;
    long port_number;
    int sock;

    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    port_number = strtol(port, 0, 10);
    if (port_number <= 0 || port_number > 65535) {
        return TG_NET_INVALID_ARGUMENT;
    }

    host_entry = gethostbyname(host);
    if (host_entry == 0 || host_entry->h_addr_list == 0 || host_entry->h_addr_list[0] == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, "host lookup failed");
        return TG_NET_RESOLVE_FAILED;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port_number);
    memcpy(&address.sin_addr, host_entry->h_addr_list[0], sizeof(address.sin_addr));

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
        return TG_NET_CONNECT_FAILED;
    }

    if (tg_platform_connect_socket(sock, &address, error_buffer,
                                   error_buffer_size) == TG_NET_OK) {
        connection->platform_handle = sock;
        connection->is_open = 1;
        return TG_NET_OK;
    }

    close(sock);
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

    rc = send((int)connection->platform_handle, data, byte_count, 0);
    if (rc < 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
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

    rc = recv((int)connection->platform_handle, buffer, buffer_size, 0);
    if (rc < 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
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

#if TG_ENABLE_TLS

static void tg_morphos_openssl_init_once(void)
{
    static int initialized = 0;

    if (!initialized) {
        SSL_library_init();
        SSL_load_error_strings();
        initialized = 1;
    }
    ERR_clear_error();
}

static void tg_platform_set_ssl_error(char *error_buffer, unsigned long error_buffer_size)
{
    unsigned long error_code;
    const char *error_string;

    error_code = ERR_get_error();
    if (error_code == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, "TLS operation failed");
        return;
    }

    error_string = ERR_reason_error_string(error_code);
    if (error_string == 0) {
        error_string = "TLS operation failed";
    }
    tg_platform_set_error(error_buffer, error_buffer_size, error_string);
}

static tg_tls_status tg_morphos_configure_certificate_validation(SSL_CTX *ctx,
                                                                SSL *ssl,
                                                                const char *host,
                                                                char *error_buffer,
                                                                unsigned long error_buffer_size)
{
    const char *ca_file;
    const char *ca_path;
    X509_VERIFY_PARAM *verify_param;

    if (!tg_tls_certificate_validation_enabled()) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);
        return TG_TLS_OK;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);
    ca_file = tg_tls_certificate_ca_file();
    ca_path = tg_tls_certificate_ca_path();
    if (ca_file != 0 || ca_path != 0) {
        if (SSL_CTX_load_verify_locations(ctx, ca_file, ca_path) != 1) {
            tg_platform_set_ssl_error(error_buffer, error_buffer_size);
            return TG_TLS_VERIFY_FAILED;
        }
    } else if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "could not load default CA paths");
        return TG_TLS_VERIFY_FAILED;
    }

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    verify_param = SSL_get0_param(ssl);
    if (verify_param == 0 ||
        X509_VERIFY_PARAM_set1_host(verify_param, host, 0) != 1) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "could not enable hostname verification");
        return TG_TLS_VERIFY_FAILED;
    }
#else
    (void)verify_param;
    tg_platform_set_error(error_buffer, error_buffer_size,
                          "hostname verification is not supported by OpenSSL");
    return TG_TLS_VERIFY_FAILED;
#endif

    return TG_TLS_OK;
}

tg_tls_status tg_platform_tls_connect(tg_tls_connection *connection, const char *host,
                                      const char *port, tg_net_status *net_status,
                                      char *error_buffer, unsigned long error_buffer_size)
{
    SSL_CTX *ctx;
    SSL *ssl;
    tg_tls_status verify_status;
    tg_net_status local_net_status;

    if (net_status != 0) {
        *net_status = TG_NET_OK;
    }

    tg_morphos_openssl_init_once();

    local_net_status = tg_net_connect(&connection->tcp, host, port,
                                      error_buffer, error_buffer_size);
    if (local_net_status != TG_NET_OK) {
        if (net_status != 0) {
            *net_status = local_net_status;
        }
        return TG_TLS_NET_ERROR;
    }

    ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size);
        tg_net_close(&connection->tcp);
        return TG_TLS_HANDSHAKE_FAILED;
    }

#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
    SSL_CTX_set_options(ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif

    ssl = SSL_new(ctx);
    if (ssl == 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size);
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        return TG_TLS_HANDSHAKE_FAILED;
    }

    SSL_set_fd(ssl, (int)connection->tcp.platform_handle);
    SSL_set_tlsext_host_name(ssl, host);
    verify_status = tg_morphos_configure_certificate_validation(
        ctx, ssl, host, error_buffer, error_buffer_size);
    if (verify_status != TG_TLS_OK) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        return verify_status;
    }

    if (SSL_connect(ssl) != 1) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        return TG_TLS_HANDSHAKE_FAILED;
    }
    if (tg_tls_certificate_validation_enabled() &&
        SSL_get_verify_result(ssl) != X509_V_OK) {
        tg_platform_set_error(
            error_buffer, error_buffer_size,
            X509_verify_cert_error_string(SSL_get_verify_result(ssl)));
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        return TG_TLS_VERIFY_FAILED;
    }

    connection->platform_context = ctx;
    connection->platform_session = ssl;
    connection->is_open = 1;
    return TG_TLS_OK;
}

tg_tls_status tg_platform_tls_send(tg_tls_connection *connection, const void *data,
                                   unsigned long byte_count, unsigned long *bytes_sent,
                                   char *error_buffer, unsigned long error_buffer_size)
{
    int rc;

    if (bytes_sent != 0) {
        *bytes_sent = 0;
    }

    rc = SSL_write((SSL *)connection->platform_session, data, (int)byte_count);
    if (rc <= 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size);
        return TG_TLS_SEND_FAILED;
    }

    if (bytes_sent != 0) {
        *bytes_sent = (unsigned long)rc;
    }
    return TG_TLS_OK;
}

tg_tls_status tg_platform_tls_recv(tg_tls_connection *connection, void *buffer,
                                   unsigned long buffer_size, unsigned long *bytes_received,
                                   char *error_buffer, unsigned long error_buffer_size)
{
    int rc;
    int ssl_error;

    if (bytes_received != 0) {
        *bytes_received = 0;
    }

    rc = SSL_read((SSL *)connection->platform_session, buffer, (int)buffer_size);
    if (rc > 0) {
        if (bytes_received != 0) {
            *bytes_received = (unsigned long)rc;
        }
        return TG_TLS_OK;
    }

    ssl_error = SSL_get_error((SSL *)connection->platform_session, rc);
    if (ssl_error == SSL_ERROR_ZERO_RETURN) {
        return TG_TLS_CLOSED;
    }

    tg_platform_set_ssl_error(error_buffer, error_buffer_size);
    return TG_TLS_RECV_FAILED;
}

void tg_platform_tls_close(tg_tls_connection *connection)
{
    if (connection == 0) {
        return;
    }
    if (connection->platform_session != 0) {
        SSL_shutdown((SSL *)connection->platform_session);
        SSL_free((SSL *)connection->platform_session);
        connection->platform_session = 0;
    }
    if (connection->platform_context != 0) {
        SSL_CTX_free((SSL_CTX *)connection->platform_context);
        connection->platform_context = 0;
    }
    tg_net_close(&connection->tcp);
}

#else

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

#endif
