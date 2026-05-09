/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef TG_AMIGAOS3_ENABLE_AMISSL
#define TG_AMIGAOS3_ENABLE_AMISSL 0
#endif

#if defined(__amigaos3__) && TG_AMIGAOS3_ENABLE_AMISSL
#ifndef __USE_NEW_TIMEVAL__
#define __USE_NEW_TIMEVAL__
#endif
#endif

#if defined(__amigaos3__)
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if defined(__amigaos3__) && TG_AMIGAOS3_ENABLE_AMISSL
#include <proto/exec.h>
/* ixemul ships BSD headers without the guard names expected by Roadshow. */
#ifndef SYS_MBUF_H
#define SYS_MBUF_H
#endif
#ifndef _SYS_MBUF_H_
#define _SYS_MBUF_H_
#endif
#ifndef NET_ROUTE_H
#define NET_ROUTE_H
#endif
#ifndef _NET_ROUTE_H_
#define _NET_ROUTE_H_
#endif
#include <proto/socket.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#include <amissl/amissl.h>
#include <libraries/amissl.h>
#include <libraries/amisslmaster.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#include "tg_platform.h"

#if defined(__amigaos3__) && TG_AMIGAOS3_ENABLE_AMISSL
struct Library *AmiSSLMasterBase = 0;
struct Library *AmiSSLBase = 0;
struct Library *AmiSSLExtBase = 0;
struct Library *SocketBase = 0;

static int tg_amigaos3_amissl_initialized = 0;

const char stack_size[] = "$STACK:65536";
#endif

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

#if TG_AMIGAOS3_ENABLE_AMISSL
static int tg_amigaos3_socket_open(char *error_buffer, unsigned long error_buffer_size)
{
    if (SocketBase != 0) {
        return 0;
    }

    SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
    if (SocketBase == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "could not open bsdsocket.library v4");
        return 1;
    }

    SetErrnoPtr(&errno, sizeof(errno));
    return 0;
}
#endif

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

#if TG_AMIGAOS3_ENABLE_AMISSL
    if (tg_amigaos3_socket_open(error_buffer, error_buffer_size) != 0) {
        return TG_NET_CONNECT_FAILED;
    }
#endif

#if TG_AMIGAOS3_ENABLE_AMISSL
    host_entry = (struct hostent *)gethostbyname((STRPTR)host);
#else
    host_entry = gethostbyname((char *)host);
#endif
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

#if TG_AMIGAOS3_ENABLE_AMISSL
    CloseSocket((long)sock);
#else
    close(sock);
#endif
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
#if TG_AMIGAOS3_ENABLE_AMISSL
        CloseSocket((long)connection->platform_handle);
        if (!tg_amigaos3_amissl_initialized && SocketBase != 0) {
            CloseLibrary(SocketBase);
            SocketBase = 0;
        }
#else
        close((int)connection->platform_handle);
#endif
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

#if defined(__amigaos3__) && TG_AMIGAOS3_ENABLE_AMISSL

static void tg_platform_set_ssl_error(char *error_buffer, unsigned long error_buffer_size,
                                      const char *fallback_message)
{
    unsigned long error_code;
    const char *error_string;

    error_code = ERR_get_error();
    if (error_code != 0) {
        error_string = ERR_reason_error_string(error_code);
        if (error_string != 0) {
            tg_platform_set_error(error_buffer, error_buffer_size, error_string);
            return;
        }
    }

    tg_platform_set_error(error_buffer, error_buffer_size, fallback_message);
}

static void tg_amigaos3_amissl_cleanup(void)
{
    if (tg_amigaos3_amissl_initialized) {
        CleanupAmiSSLA(0);
        tg_amigaos3_amissl_initialized = 0;
    }

    if (AmiSSLBase != 0) {
        CloseAmiSSL();
        AmiSSLBase = 0;
        AmiSSLExtBase = 0;
    }

    if (AmiSSLMasterBase != 0) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = 0;
    }

    if (SocketBase != 0) {
        CloseLibrary(SocketBase);
        SocketBase = 0;
    }
}

static int tg_amigaos3_amissl_init(char *error_buffer, unsigned long error_buffer_size)
{
    long amissl_error;
    char detail[80];

    if (tg_amigaos3_amissl_initialized) {
        return 0;
    }

    if (tg_amigaos3_socket_open(error_buffer, error_buffer_size) != 0) {
        tg_amigaos3_amissl_cleanup();
        return 1;
    }

    AmiSSLMasterBase = OpenLibrary((CONST_STRPTR)"amisslmaster.library",
                                   AMISSLMASTER_MIN_VERSION);
    if (AmiSSLMasterBase == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "could not open amisslmaster.library");
        tg_amigaos3_amissl_cleanup();
        return 1;
    }

    amissl_error = OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
                                  AmiSSL_UsesOpenSSLStructs, FALSE,
                                  AmiSSL_InitAmiSSL, TRUE,
                                  AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
                                  AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
                                  AmiSSL_SocketBase, (ULONG)SocketBase,
                                  AmiSSL_ErrNoPtr, (ULONG)&errno,
                                  TAG_DONE);
    if (amissl_error != 0) {
        sprintf(detail, "could not initialize AmiSSL (%ld)", amissl_error);
        tg_platform_set_error(error_buffer, error_buffer_size, detail);
        tg_amigaos3_amissl_cleanup();
        return 1;
    }

    tg_amigaos3_amissl_initialized = 1;
    OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS |
                         OPENSSL_INIT_ADD_ALL_DIGESTS,
                     0);
    return 0;
}

tg_tls_status tg_platform_tls_connect(tg_tls_connection *connection, const char *host,
                                      const char *port, tg_net_status *net_status,
                                      char *error_buffer, unsigned long error_buffer_size)
{
    SSL_CTX *ctx;
    SSL *ssl;
    tg_net_status local_net_status;

    if (net_status != 0) {
        *net_status = TG_NET_OK;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    if (tg_amigaos3_amissl_init(error_buffer, error_buffer_size) != 0) {
        return TG_TLS_HANDSHAKE_FAILED;
    }

    local_net_status = tg_net_connect(&connection->tcp, host, port,
                                      error_buffer, error_buffer_size);
    if (local_net_status != TG_NET_OK) {
        if (net_status != 0) {
            *net_status = local_net_status;
        }
        tg_amigaos3_amissl_cleanup();
        return TG_TLS_NET_ERROR;
    }

    ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "could not create SSL context");
        tg_net_close(&connection->tcp);
        tg_amigaos3_amissl_cleanup();
        return TG_TLS_HANDSHAKE_FAILED;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);
#ifdef SSL_MODE_AUTO_RETRY
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
#endif

    ssl = SSL_new(ctx);
    if (ssl == 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "could not create SSL session");
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        tg_amigaos3_amissl_cleanup();
        return TG_TLS_HANDSHAKE_FAILED;
    }

    SSL_set_fd(ssl, (int)connection->tcp.platform_handle);
    SSL_set_tlsext_host_name(ssl, host);
    ERR_clear_error();

    if (SSL_connect(ssl) != 1) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "SSL handshake failed");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        tg_amigaos3_amissl_cleanup();
        return TG_TLS_HANDSHAKE_FAILED;
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
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    rc = SSL_write((SSL *)connection->platform_session, data, (int)byte_count);
    if (rc <= 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "SSL write failed");
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
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    rc = SSL_read((SSL *)connection->platform_session, buffer, (int)buffer_size);
    if (rc > 0) {
        if (bytes_received != 0) {
            *bytes_received = (unsigned long)rc;
        }
        return TG_TLS_OK;
    }

    ssl_error = SSL_get_error((SSL *)connection->platform_session, rc);
    if (rc == 0 || ssl_error == SSL_ERROR_ZERO_RETURN) {
        return TG_TLS_CLOSED;
    }

    tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                              "SSL read failed");
    return TG_TLS_RECV_FAILED;
}

void tg_platform_tls_close(tg_tls_connection *connection)
{
    SSL *ssl;
    SSL_CTX *ctx;

    if (connection == 0) {
        return;
    }

    ssl = (SSL *)connection->platform_session;
    ctx = (SSL_CTX *)connection->platform_context;
    if (ssl != 0) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    if (ctx != 0) {
        SSL_CTX_free(ctx);
    }

    tg_net_close(&connection->tcp);
    connection->platform_session = 0;
    connection->platform_context = 0;
    connection->is_open = 0;
    tg_amigaos3_amissl_cleanup();
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
