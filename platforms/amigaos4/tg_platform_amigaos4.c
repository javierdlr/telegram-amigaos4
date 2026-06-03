/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if !defined(__amigaos4__)
#include <fcntl.h>
#endif

#ifndef TG_AMIGAOS4_ENABLE_AMISSL
#define TG_AMIGAOS4_ENABLE_AMISSL 0
#endif

#if defined(__amigaos4__)
#ifndef __USE_INLINE__
#define __USE_INLINE__
#endif
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/socket.h>
#endif

#if defined(__amigaos4__) && TG_AMIGAOS4_ENABLE_AMISSL
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#include <amissl/amissl.h>
#include <libraries/amissl.h>
#include <libraries/amisslmaster.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#endif

#include "tg_platform.h"
#include "tg_mtproto_crypto.h"

#if defined(__amigaos4__)
struct Library *SocketBase = 0;
struct SocketIFace *ISocket = 0;
#endif

#if defined(__amigaos4__) && TG_AMIGAOS4_ENABLE_AMISSL
struct Library *AmiSSLMasterBase = 0;
struct AmiSSLMasterIFace *IAmiSSLMaster = 0;
struct AmiSSLIFace *IAmiSSL = 0;

static int tg_amigaos4_amissl_initialized = 0;

static int tg_amigaos4_amissl_init(char *error_buffer,
                                   unsigned long error_buffer_size);
#endif

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

void tg_platform_sleep_seconds(unsigned long seconds)
{
#if defined(__amigaos4__)
    if (seconds > 0) {
        sleep(seconds);
    }
#else
    if (seconds > 0) {
        sleep((unsigned int)seconds);
    }
#endif
}

int tg_platform_stdin_readable(unsigned long timeout_seconds)
{
#if defined(__amigaos4__)
    unsigned long long timeout_microseconds;

    timeout_microseconds = (unsigned long long)timeout_seconds * 1000000ULL;
    if (timeout_microseconds > 2147000000ULL) {
        timeout_microseconds = 2147000000ULL;
    }
    return WaitForChar(Input(), (long)timeout_microseconds) != 0;
#else
    (void)timeout_seconds;
    return 0;
#endif
}

int tg_platform_stdin_read_char(unsigned long timeout_seconds, char *out_char)
{
#if defined(__amigaos4__)
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
#else
    (void)timeout_seconds;
    (void)out_char;
    return 0;
#endif
}

int tg_platform_stdin_read_hidden_line(char *out, unsigned long out_size)
{
#if defined(__amigaos4__)
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
#else
    if (out != 0 && out_size > 0UL) {
        out[0] = '\0';
    }
    return -1;
#endif
}

int tg_platform_stdin_set_raw(int enabled)
{
#if defined(__amigaos4__)
    if (SetMode(Input(), enabled ? 1 : 0)) {
        return 0;
    }
    return -1;
#else
    (void)enabled;
    return -1;
#endif
}

#if defined(__amigaos4__)
/*
 * In-tree CSPRNG for AmigaOS 4.x (SHA-256 Hash-DRBG).
 *
 * AmiSSL's first RAND_bytes() call triggers OpenSSL provider initialisation
 * and DRBG (re)seeding, which is pathologically slow on OS4 / emulated PPC
 * (measured ~2 minutes before the first random byte is produced). MTProto on
 * OS4 uses the in-tree big-integer and SHA implementations for all of its
 * cryptography, so we avoid AmiSSL entirely for randomness: we gather local
 * entropy and run it through SHA-256 in a Hash-DRBG construction.
 *
 * Entropy sources: PPC time-base register (high resolution, jitter), wall
 * clock (gettimeofday + time), several run-varying addresses and a per-call
 * counter. Each call mixes fresh entropy into a working key, emits output as
 * SHA-256(key || block-counter), then ratchets the persistent state forward
 * (SHA-256(key || 0x01)) so prior output cannot be reconstructed.
 */
static unsigned char tg_os4_drbg_state[TG_MTPROTO_SHA256_LENGTH];
static int tg_os4_drbg_ready = 0;

static unsigned long tg_os4_timebase(void)
{
    unsigned long tb = 0;
#if defined(__GNUC__)
    __asm__ volatile("mftb %0" : "=r"(tb));
#endif
    return tb;
}

static unsigned long tg_os4_entropy_gather(unsigned char *buf, unsigned long cap)
{
    unsigned long n = 0;
    unsigned long i;
    struct timeval tv;
    time_t now;
    void *p;
    struct Task *task;

    now = time(0);
    if (n + sizeof(now) <= cap) {
        memcpy(buf + n, &now, sizeof(now));
        n += sizeof(now);
    }

    /* High-resolution time-base sampled in a variable-duration jitter loop. */
    for (i = 0; i < 96UL && n + sizeof(unsigned long) <= cap; ++i) {
        unsigned long tb = tg_os4_timebase();
        volatile unsigned long spin;
        unsigned long k;
        memcpy(buf + n, &tb, sizeof(tb));
        n += sizeof(tb);
        spin = tb;
        for (k = 0; k < (tb & 0x3fUL); ++k) {
            spin = (spin * 2654435761UL) + k;
        }
        (void)spin;
        if (n + sizeof(tv) <= cap) {
            gettimeofday(&tv, 0);
            memcpy(buf + n, &tv, sizeof(tv));
            n += sizeof(tv);
        }
    }

    p = (void *)&tv;
    if (n + sizeof(p) <= cap) { memcpy(buf + n, &p, sizeof(p)); n += sizeof(p); }
    p = (void *)buf;
    if (n + sizeof(p) <= cap) { memcpy(buf + n, &p, sizeof(p)); n += sizeof(p); }
    p = (void *)IExec;
    if (n + sizeof(p) <= cap) { memcpy(buf + n, &p, sizeof(p)); n += sizeof(p); }
    task = FindTask(0);
    if (n + sizeof(task) <= cap) { memcpy(buf + n, &task, sizeof(task)); n += sizeof(task); }

    return n;
}

static void tg_os4_drbg_seed(void)
{
    unsigned char pool[1024];
    unsigned long len = tg_os4_entropy_gather(pool, sizeof(pool));
    tg_mtproto_sha256(pool, len, tg_os4_drbg_state);
    tg_os4_drbg_ready = 1;
}

static void tg_os4_drbg_generate(unsigned char *out, unsigned long n)
{
    static unsigned long calls = 0;
    unsigned char key[TG_MTPROTO_SHA256_LENGTH];
    unsigned char block[TG_MTPROTO_SHA256_LENGTH];
    unsigned long off;
    unsigned long ctr;

    if (!tg_os4_drbg_ready) {
        tg_os4_drbg_seed();
    }

    /* Derive a per-call working key from the state plus fresh entropy. */
    {
        unsigned char work[TG_MTPROTO_SHA256_LENGTH + 32];
        unsigned long wn = TG_MTPROTO_SHA256_LENGTH;
        struct timeval tv;
        unsigned long tb = tg_os4_timebase();
        memcpy(work, tg_os4_drbg_state, TG_MTPROTO_SHA256_LENGTH);
        gettimeofday(&tv, 0);
        ++calls;
        memcpy(work + wn, &tv, sizeof(tv)); wn += sizeof(tv);
        memcpy(work + wn, &tb, sizeof(tb)); wn += sizeof(tb);
        memcpy(work + wn, &calls, sizeof(calls)); wn += sizeof(calls);
        tg_mtproto_sha256(work, wn, key);
    }

    off = 0;
    ctr = 0;
    while (off < n) {
        unsigned char cb[TG_MTPROTO_SHA256_LENGTH + 4];
        unsigned long take;
        memcpy(cb, key, TG_MTPROTO_SHA256_LENGTH);
        cb[TG_MTPROTO_SHA256_LENGTH + 0] = (unsigned char)((ctr >> 24) & 0xffUL);
        cb[TG_MTPROTO_SHA256_LENGTH + 1] = (unsigned char)((ctr >> 16) & 0xffUL);
        cb[TG_MTPROTO_SHA256_LENGTH + 2] = (unsigned char)((ctr >> 8) & 0xffUL);
        cb[TG_MTPROTO_SHA256_LENGTH + 3] = (unsigned char)(ctr & 0xffUL);
        tg_mtproto_sha256(cb, sizeof(cb), block);
        take = n - off;
        if (take > TG_MTPROTO_SHA256_LENGTH) {
            take = TG_MTPROTO_SHA256_LENGTH;
        }
        memcpy(out + off, block, take);
        off += take;
        ++ctr;
    }

    /* Ratchet the persistent state forward. */
    {
        unsigned char rb[TG_MTPROTO_SHA256_LENGTH + 1];
        memcpy(rb, key, TG_MTPROTO_SHA256_LENGTH);
        rb[TG_MTPROTO_SHA256_LENGTH] = 0x01;
        tg_mtproto_sha256(rb, sizeof(rb), tg_os4_drbg_state);
    }
}
#endif /* __amigaos4__ */

int tg_platform_random_bytes(unsigned char *bytes, unsigned long byte_count)
{
#if defined(__amigaos4__)
    if (bytes == 0) {
        return 0;
    }
    if (byte_count == 0) {
        return 1;
    }
    tg_os4_drbg_generate(bytes, byte_count);
    return 1;
#else
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
    if (fd < 0) {
        return 0;
    }
    offset = 0;
    while (offset < byte_count) {
        got = read(fd, bytes + offset, byte_count - offset);
        if (got <= 0) {
            close(fd);
            return 0;
        }
        offset += (unsigned long)got;
    }
    close(fd);
    return 1;
#endif
}

#if defined(__amigaos4__)

static void tg_platform_set_error(char *error_buffer, unsigned long error_buffer_size,
                                  const char *message)
{
    if (error_buffer != 0 && error_buffer_size > 0) {
        strncpy(error_buffer, message, error_buffer_size - 1);
        error_buffer[error_buffer_size - 1] = '\0';
    }
}

static int tg_amigaos4_socket_open(char *error_buffer, unsigned long error_buffer_size)
{
    if (SocketBase != 0 && ISocket != 0) {
        return 0;
    }

    SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
    if (SocketBase == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "could not open bsdsocket.library v4");
        return 1;
    }

    ISocket = (struct SocketIFace *)GetInterface((struct Library *)SocketBase,
                                                 "main", 1L, 0);
    if (ISocket == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "could not get bsdsocket.library interface");
        CloseLibrary(SocketBase);
        SocketBase = 0;
        return 1;
    }

    return 0;
}

static void tg_amigaos4_socket_close_library(void)
{
    if (ISocket != 0) {
        DropInterface((struct Interface *)ISocket);
        ISocket = 0;
    }
    if (SocketBase != 0) {
        CloseLibrary(SocketBase);
        SocketBase = 0;
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

    if (tg_amigaos4_socket_open(error_buffer, error_buffer_size) != 0) {
        return TG_NET_CONNECT_FAILED;
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
    address.sin_len = (unsigned char)sizeof(address);
    memcpy(&address.sin_addr, host_entry->h_addr_list[0], (size_t)host_entry->h_length);

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

    CloseSocket(sock);
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

    rc = send((int)connection->platform_handle, (void *)data, (long)byte_count, 0);
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
    unsigned long timeout_seconds;
    fd_set read_fds;
    struct timeval timeout;
    long rc;

    if (bytes_received != 0) {
        *bytes_received = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    /* Bound the blocking recv() with a WaitSelect() timeout, mirroring the
       MorphOS/AROS backends. Without this the encrypted-query receive loop in
       tg_mtproto_send_saved_query_on_context() can block forever inside recv()
       when the server goes quiet (e.g. a contacts.search from /add with no
       prompt reply): its 12s wall-clock budget is only checked between reads,
       so a stuck recv() never lets it fire and the chat hangs after /add. */
    timeout_seconds = tg_net_connect_timeout_seconds();
    if (timeout_seconds == 0UL) {
        timeout_seconds = 30UL;
    }
    FD_ZERO(&read_fds);
    FD_SET((int)connection->platform_handle, &read_fds);
    timeout.tv_sec = (long)timeout_seconds;
    timeout.tv_usec = 0;
    rc = WaitSelect((int)connection->platform_handle + 1, &read_fds, 0, 0,
                    &timeout, 0);
    if (rc <= 0 || !FD_ISSET((int)connection->platform_handle, &read_fds)) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              rc == 0 ? "socket receive timed out"
                                      : "socket receive failed");
        return rc == 0 ? TG_NET_TIMEOUT : TG_NET_RECV_FAILED;
    }

    rc = recv((int)connection->platform_handle, buffer, (long)buffer_size, 0);
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
        CloseSocket((int)connection->platform_handle);
#if !TG_AMIGAOS4_ENABLE_AMISSL
        tg_amigaos4_socket_close_library();
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

#if defined(__amigaos4__) && TG_AMIGAOS4_ENABLE_AMISSL

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

static tg_tls_status tg_amigaos4_configure_certificate_validation(
    SSL_CTX *ctx,
    SSL *ssl,
    const char *host,
    char *error_buffer,
    unsigned long error_buffer_size)
{
    const char *ca_file;
    const char *ca_path;

    if (!tg_tls_certificate_validation_enabled()) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);
        return TG_TLS_OK;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);
    ca_file = tg_tls_certificate_ca_file();
    ca_path = tg_tls_certificate_ca_path();
    if (ca_file != 0 || ca_path != 0) {
        if (SSL_CTX_load_verify_locations(ctx, ca_file, ca_path) != 1) {
            tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                      "could not load CA file/path");
            return TG_TLS_VERIFY_FAILED;
        }
    } else if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "could not load default CA paths");
        return TG_TLS_VERIFY_FAILED;
    }

    if (SSL_set1_host(ssl, host) != 1) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "could not enable hostname verification");
        return TG_TLS_VERIFY_FAILED;
    }

    return TG_TLS_OK;
}

static void tg_amigaos4_amissl_cleanup(void)
{
    if (tg_amigaos4_amissl_initialized) {
        CleanupAmiSSLA(0);
        tg_amigaos4_amissl_initialized = 0;
    }

    if (IAmiSSL != 0) {
        CloseAmiSSL();
        IAmiSSL = 0;
    }

    if (IAmiSSLMaster != 0) {
        DropInterface((struct Interface *)IAmiSSLMaster);
        IAmiSSLMaster = 0;
    }

    if (AmiSSLMasterBase != 0) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = 0;
    }

    tg_amigaos4_socket_close_library();
}

static int tg_amigaos4_amissl_init(char *error_buffer, unsigned long error_buffer_size)
{
    long amissl_error;
    char detail[80];

    if (tg_amigaos4_amissl_initialized) {
        return 0;
    }

    if (tg_amigaos4_socket_open(error_buffer, error_buffer_size) != 0) {
        tg_amigaos4_amissl_cleanup();
        return 1;
    }

    AmiSSLMasterBase = OpenLibrary((CONST_STRPTR)"amisslmaster.library",
                                   AMISSLMASTER_MIN_VERSION);
    if (AmiSSLMasterBase == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "could not open amisslmaster.library");
        tg_amigaos4_amissl_cleanup();
        return 1;
    }

    IAmiSSLMaster = (struct AmiSSLMasterIFace *)
        GetInterface((struct Library *)AmiSSLMasterBase, "main", 1L, 0);
    if (IAmiSSLMaster == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "could not get AmiSSLMaster interface");
        tg_amigaos4_amissl_cleanup();
        return 1;
    }

    amissl_error = OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
                                  AmiSSL_UsesOpenSSLStructs, FALSE,
                                  AmiSSL_GetIAmiSSL, &IAmiSSL,
                                  AmiSSL_ISocket, ISocket,
                                  AmiSSL_ErrNoPtr, &errno,
                                  TAG_DONE);
    if (amissl_error != 0) {
        sprintf(detail, "could not initialize AmiSSL (%ld)", amissl_error);
        tg_platform_set_error(error_buffer, error_buffer_size, detail);
        tg_amigaos4_amissl_cleanup();
        return 1;
    }

    tg_amigaos4_amissl_initialized = 1;
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
    tg_tls_status verify_status;
    tg_net_status local_net_status;

    if (net_status != 0) {
        *net_status = TG_NET_OK;
    }

    if (tg_amigaos4_amissl_init(error_buffer, error_buffer_size) != 0) {
        return TG_TLS_HANDSHAKE_FAILED;
    }

    local_net_status = tg_net_connect(&connection->tcp, host, port,
                                      error_buffer, error_buffer_size);
    if (local_net_status != TG_NET_OK) {
        if (net_status != 0) {
            *net_status = local_net_status;
        }
        tg_amigaos4_amissl_cleanup();
        return TG_TLS_NET_ERROR;
    }

    ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "TLS context creation failed");
        tg_net_close(&connection->tcp);
        tg_amigaos4_amissl_cleanup();
        return TG_TLS_HANDSHAKE_FAILED;
    }

#ifdef SSL_MODE_AUTO_RETRY
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
#endif
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
    SSL_CTX_set_options(ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif

    ssl = SSL_new(ctx);
    if (ssl == 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "TLS session creation failed");
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        tg_amigaos4_amissl_cleanup();
        return TG_TLS_HANDSHAKE_FAILED;
    }

    SSL_set_fd(ssl, (int)connection->tcp.platform_handle);
    SSL_set_tlsext_host_name(ssl, host);
    verify_status = tg_amigaos4_configure_certificate_validation(
        ctx, ssl, host, error_buffer, error_buffer_size);
    if (verify_status != TG_TLS_OK) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        tg_amigaos4_amissl_cleanup();
        return verify_status;
    }
    ERR_clear_error();

    if (SSL_connect(ssl) != 1) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size,
                                  "TLS handshake failed");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        tg_net_close(&connection->tcp);
        tg_amigaos4_amissl_cleanup();
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
        tg_amigaos4_amissl_cleanup();
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
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    rc = SSL_write((SSL *)connection->platform_session, data, (int)byte_count);
    if (rc <= 0) {
        tg_platform_set_ssl_error(error_buffer, error_buffer_size, "TLS send failed");
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
    if (ssl_error == SSL_ERROR_ZERO_RETURN) {
        return TG_TLS_CLOSED;
    }

    tg_platform_set_ssl_error(error_buffer, error_buffer_size, "TLS receive failed");
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
    connection->platform_session = 0;
    connection->platform_context = 0;
    connection->is_open = 0;
    tg_amigaos4_amissl_cleanup();
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
