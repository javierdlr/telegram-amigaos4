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
#include <time.h>
#include <unistd.h>
#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <proto/timer.h>

#ifndef TG_ENABLE_TLS
#define TG_ENABLE_TLS 0
#endif

#if TG_ENABLE_TLS
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#endif

#include "tg_platform.h"
#include "tg_mtproto_crypto.h"

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

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

int tg_platform_stdin_read_char(unsigned long timeout_seconds, char *out_char)
{
    unsigned long long timeout_microseconds;
    char ch;
    LONG got;

    if (out_char == 0) {
        return -1;
    }
    timeout_microseconds = (unsigned long long)timeout_seconds * 1000000ULL;
    if (timeout_microseconds > 2147000000ULL) {
        timeout_microseconds = 2147000000ULL;
    }
    if (WaitForChar(Input(), (long)timeout_microseconds) == 0) {
        return 0;
    }
    got = Read(Input(), &ch, 1);
    if (got <= 0) {
        return -1;
    }
    *out_char = ch;
    return 1;
}

int tg_platform_stdin_read_hidden_line(char *out, unsigned long out_size)
{
    unsigned long pos;
    char ch;
    LONG got;

    if (out == 0 || out_size == 0UL) {
        return -1;
    }
    out[0] = '\0';
    pos = 0UL;
    SetMode(Input(), 1);    /* RAW console: no echo, no line editing */
    for (;;) {
        got = Read(Input(), &ch, 1);
        if (got <= 0) {
            SetMode(Input(), 0);
            return -1;
        }
        if (ch == '\n' || ch == '\r') {
            break;
        }
        if (ch == '\b' || ch == 0x7f) {
            if (pos > 0UL) {
                --pos;
            }
            continue;
        }
        if (pos + 1UL < out_size) {
            out[pos++] = ch;
        }
    }
    out[pos] = '\0';
    SetMode(Input(), 0);    /* restore cooked mode */
    return 0;
}

int tg_platform_stdin_set_raw(int enabled)
{
    if (SetMode(Input(), enabled ? 1 : 0)) {
        return 0;
    }
    return -1;
}

/*
 * MorphOS ships no /dev/urandom and the default build links no TLS, so neither
 * the device path nor OpenSSL's RAND_bytes is available; the previous code then
 * returned failure and the MTProto login aborted with "secure-rng-unavailable".
 * Provide a self-contained entropy source: sample the high-resolution E-Clock
 * (timer.device) plus several time/memory/address sources, whiten everything
 * through SHA-512 and run a SHA-512 hash-chain DRBG. /dev/urandom (should a
 * future MorphOS expose it) and OpenSSL RAND_bytes stay preferred.
 */
struct Library *TimerBase = 0; /* declared (extern) by <proto/timer.h> */
static struct MsgPort *tg_morphos_timer_port = 0;
static struct timerequest *tg_morphos_timer_req = 0;
static int tg_morphos_timer_state = -1; /* -1 untried, 0 unavailable, 1 ready */

static void tg_morphos_timer_open(void)
{
    if (tg_morphos_timer_state >= 0) {
        return;
    }
    tg_morphos_timer_state = 0;
    tg_morphos_timer_port = CreateMsgPort();
    if (tg_morphos_timer_port == 0) {
        return;
    }
    tg_morphos_timer_req = (struct timerequest *)CreateIORequest(
        tg_morphos_timer_port, (ULONG)sizeof(struct timerequest));
    if (tg_morphos_timer_req == 0) {
        DeleteMsgPort(tg_morphos_timer_port);
        tg_morphos_timer_port = 0;
        return;
    }
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)tg_morphos_timer_req, 0L) != 0) {
        DeleteIORequest((struct IORequest *)tg_morphos_timer_req);
        DeleteMsgPort(tg_morphos_timer_port);
        tg_morphos_timer_req = 0;
        tg_morphos_timer_port = 0;
        return;
    }
    TimerBase = (struct Library *)tg_morphos_timer_req->tr_node.io_Device;
    tg_morphos_timer_state = 1;
}

static void tg_morphos_fold(unsigned char *pool, unsigned long pool_size,
                            unsigned long *pos, unsigned long value)
{
    unsigned int b;

    for (b = 0; b < (unsigned int)sizeof(unsigned long); ++b) {
        pool[(*pos) % pool_size] ^= (unsigned char)(value & 0xffUL);
        value >>= 8;
        ++(*pos);
    }
}

static void tg_morphos_gather(unsigned char digest[TG_MTPROTO_SHA512_LENGTH])
{
    unsigned char pool[128];
    unsigned long pos;
    unsigned long i;
    struct timeval tv;
    struct DateStamp ds;
    struct EClockVal ev;
    volatile unsigned long spin;

    memset(pool, 0, sizeof(pool));
    pos = 0;
    /* Address-layout and coarse system state. */
    tg_morphos_fold(pool, sizeof(pool), &pos, (unsigned long)pool);
    tg_morphos_fold(pool, sizeof(pool), &pos, (unsigned long)&pos);
    tg_morphos_fold(pool, sizeof(pool), &pos, (unsigned long)digest);
    tg_morphos_fold(pool, sizeof(pool), &pos, AvailMem(MEMF_ANY));
    tg_morphos_fold(pool, sizeof(pool), &pos, AvailMem(MEMF_CHIP));
    tg_morphos_fold(pool, sizeof(pool), &pos, AvailMem(MEMF_FAST));
    tg_morphos_fold(pool, sizeof(pool), &pos, (unsigned long)time(0));
    tg_morphos_fold(pool, sizeof(pool), &pos, (unsigned long)clock());
    memset(&ds, 0, sizeof(ds));
    DateStamp(&ds);
    tg_morphos_fold(pool, sizeof(pool), &pos, (unsigned long)ds.ds_Days);
    tg_morphos_fold(pool, sizeof(pool), &pos, (unsigned long)ds.ds_Minute);
    tg_morphos_fold(pool, sizeof(pool), &pos, (unsigned long)ds.ds_Tick);
    /* Time-jitter loop: read the high-resolution clocks while doing a variable
       amount of work so scheduling/interrupt jitter perturbs the samples. */
    spin = 1;
    for (i = 0; i < 64UL; ++i) {
        unsigned long s;

        if (gettimeofday(&tv, 0) == 0) {
            tg_morphos_fold(pool, sizeof(pool), &pos,
                            (unsigned long)tv.tv_sec);
            tg_morphos_fold(pool, sizeof(pool), &pos,
                            (unsigned long)tv.tv_usec);
        }
        if (tg_morphos_timer_state == 1) {
            ReadEClock(&ev);
            tg_morphos_fold(pool, sizeof(pool), &pos, ev.ev_lo);
            tg_morphos_fold(pool, sizeof(pool), &pos, ev.ev_hi);
        }
        for (s = 0; s < (spin & 0x3fUL); ++s) {
            spin += (spin << 2) + 0x9e3779b9UL + i;
        }
    }
    tg_mtproto_sha512(pool, sizeof(pool), digest);
    memset(pool, 0, sizeof(pool));
}

static int tg_morphos_random_fallback(unsigned char *bytes,
                                      unsigned long byte_count)
{
    static unsigned char state[TG_MTPROTO_SHA512_LENGTH];
    static int seeded = 0;
    unsigned char fresh[TG_MTPROTO_SHA512_LENGTH];
    unsigned char block[TG_MTPROTO_SHA512_LENGTH];
    unsigned char mix[2 * TG_MTPROTO_SHA512_LENGTH];
    unsigned long produced;
    unsigned long counter;
    unsigned long chunk;
    unsigned int j;

    tg_morphos_timer_open();
    tg_morphos_gather(fresh);
    if (!seeded) {
        memcpy(state, fresh, sizeof(state));
        seeded = 1;
    }
    /* state := SHA512(state || fresh) -- absorb new entropy each call. */
    memcpy(mix, state, TG_MTPROTO_SHA512_LENGTH);
    memcpy(mix + TG_MTPROTO_SHA512_LENGTH, fresh, TG_MTPROTO_SHA512_LENGTH);
    tg_mtproto_sha512(mix, sizeof(mix), state);

    produced = 0;
    counter = 0;
    while (produced < byte_count) {
        unsigned long c;

        memcpy(mix, state, TG_MTPROTO_SHA512_LENGTH);
        c = counter;
        for (j = 0; j < 8U; ++j) {
            mix[TG_MTPROTO_SHA512_LENGTH + j] = (unsigned char)(c & 0xffUL);
            c >>= 8;
        }
        tg_mtproto_sha512(mix, TG_MTPROTO_SHA512_LENGTH + 8UL, block);
        chunk = byte_count - produced;
        if (chunk > (unsigned long)TG_MTPROTO_SHA512_LENGTH) {
            chunk = (unsigned long)TG_MTPROTO_SHA512_LENGTH;
        }
        memcpy(bytes + produced, block, chunk);
        produced += chunk;
        ++counter;
    }
    /* Ratchet the state so a later call differs even without fresh entropy. */
    memcpy(mix, state, TG_MTPROTO_SHA512_LENGTH);
    mix[TG_MTPROTO_SHA512_LENGTH] = 0x01;
    tg_mtproto_sha512(mix, TG_MTPROTO_SHA512_LENGTH + 1UL, state);

    memset(fresh, 0, sizeof(fresh));
    memset(block, 0, sizeof(block));
    memset(mix, 0, sizeof(mix));
    return 1;
}

int tg_platform_random_bytes(unsigned char *bytes, unsigned long byte_count)
{
    int fd;
    unsigned long offset;
    long got;

    if (bytes == 0) {
        return 0;
    }
    if (byte_count == 0) {
        return 1;
    }
    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        fd = open("/dev/random", O_RDONLY);
    }
    if (fd >= 0) {
        offset = 0;
        while (offset < byte_count) {
            got = read(fd, bytes + offset, byte_count - offset);
            if (got <= 0) {
                break;
            }
            offset += (unsigned long)got;
        }
        close(fd);
        if (offset >= byte_count) {
            return 1;
        }
    }
#if TG_ENABLE_TLS
    if (RAND_bytes(bytes, (int)byte_count) == 1) {
        return 1;
    }
#endif
    return tg_morphos_random_fallback(bytes, byte_count);
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

    host_entry = gethostbyname((const unsigned char *)host);
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
    unsigned long timeout_seconds;
    fd_set read_fds;
    struct timeval timeout;
    long rc;

    if (bytes_received != 0) {
        *bytes_received = 0;
    }

    timeout_seconds = tg_net_connect_timeout_seconds();
    if (timeout_seconds == 0UL) {
        timeout_seconds = 30UL;
    }
    FD_ZERO(&read_fds);
    FD_SET((int)connection->platform_handle, &read_fds);
    timeout.tv_sec = (long)timeout_seconds;
    timeout.tv_usec = 0;
    rc = select((int)connection->platform_handle + 1, &read_fds, 0, 0,
                &timeout);
    if (rc <= 0 || !FD_ISSET((int)connection->platform_handle, &read_fds)) {
        if (rc == 0) {
            tg_platform_set_error(error_buffer, error_buffer_size,
                                  "socket receive timed out");
        } else {
            tg_platform_set_error(error_buffer, error_buffer_size,
                                  strerror(errno));
        }
        return rc == 0 ? TG_NET_TIMEOUT : TG_NET_RECV_FAILED;
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
        (void)shutdown((int)connection->platform_handle, SHUT_RDWR);
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

int tg_platform_break_pending(void)
{
    /* Peek without clearing: the break stays pending for outer loops. */
    return (SetSignal(0L, 0L) & SIGBREAKF_CTRL_C) != 0L;
}

#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase = 0;

void tg_platform_display_beep(void)
{
    /* The screen flash is the Amiga-native notification; a BEL byte lets
       console handlers improvise (AmiKit's console clears the window). */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library",
                                                        0L);
    if (IntuitionBase != 0) {
        DisplayBeep(0L);
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = 0;
    }
}
