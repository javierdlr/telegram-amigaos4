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

#ifndef TG_ENABLE_TLS
#define TG_ENABLE_TLS 0
#endif

#if defined(__AROS__)
#include <exec/libraries.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dos.h>

struct Library *SocketBase = 0;
#else
#include <termios.h>
#endif

#if TG_ENABLE_TLS
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#endif

#include <time.h>

#include "tg_platform.h"
#include "tg_mtproto_crypto.h"

const char *tg_platform_name(void)
{
    return "AROS";
}

const char *tg_platform_default_data_dir(void)
{
    return "PROGDIR:";
}

void tg_platform_log(const char *level, const char *message)
{
    printf("[aros:%s] %s\n", level, message);
}

void tg_platform_debug(const char *message)
{
    (void)message; /* no dedicated kernel-debug channel used here */
}

void tg_platform_sleep_seconds(unsigned long seconds)
{
    if (seconds > 0) {
        sleep(seconds);
    }
}

int tg_platform_stdin_readable(unsigned long timeout_seconds)
{
#if defined(__AROS__)
    unsigned long long timeout_microseconds;

    timeout_microseconds = (unsigned long long)timeout_seconds * 1000000ULL;
    if (timeout_microseconds > 2147000000ULL) {
        timeout_microseconds = 2147000000ULL;
    }
    return WaitForChar(Input(), (long)timeout_microseconds) != 0;
#else
    fd_set read_fds;
    struct timeval timeout;
    int rc;

    FD_ZERO(&read_fds);
    FD_SET(0, &read_fds);
    timeout.tv_sec = (long)timeout_seconds;
    timeout.tv_usec = 0;
    rc = select(1, &read_fds, 0, 0, &timeout);
    return rc > 0 && FD_ISSET(0, &read_fds);
#endif
}

/*
 * Keystroke-timing entropy: every byte read from the console folds its
 * arrival time into a small ring at O(1) cost (no hashing on the input
 * path); the DRBG absorbs the ring on every generate. Human typing right
 * before the MTProto auth-key DH is exactly the entropy a virtualised TSC
 * lacks, and it was being thrown away.
 */
static unsigned long tg_aros_timebase(void);

static unsigned long tg_aros_key_ring[16];
static unsigned long tg_aros_key_ring_pos = 0;

static void tg_aros_note_input_event(int ch)
{
    unsigned long v = tg_aros_timebase() ^
                      ((unsigned long)(unsigned char)ch << 24) ^
                      (tg_aros_key_ring_pos * 2654435761UL);
    tg_aros_key_ring[tg_aros_key_ring_pos & 15UL] ^=
        (v << (tg_aros_key_ring_pos & 7UL)) ^ (v >> 5);
    ++tg_aros_key_ring_pos;
}

int tg_platform_stdin_read_char(unsigned long timeout_seconds, char *out_char)
{
#if defined(__AROS__)
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
    tg_aros_note_input_event((int)ch);
    *out_char = ch;
    return 1;
#else
    fd_set read_fds;
    struct timeval timeout;
    char ch;
    int rc;
    ssize_t got;

    if (out_char == 0) {
        return -1;
    }
    FD_ZERO(&read_fds);
    FD_SET(0, &read_fds);
    timeout.tv_sec = (long)timeout_seconds;
    timeout.tv_usec = 0;
    rc = select(1, &read_fds, 0, 0, &timeout);
    if (rc <= 0 || !FD_ISSET(0, &read_fds)) {
        return 0;
    }
    got = read(0, &ch, 1);
    if (got <= 0) {
        return -1;
    }
    tg_aros_note_input_event((int)ch);
    *out_char = ch;
    return 1;
#endif
}

int tg_platform_stdin_read_hidden_line(char *out, unsigned long out_size)
{
#if defined(__AROS__)
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
        tg_aros_note_input_event((int)ch);
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
    struct termios old_term;
    struct termios new_term;
    int have_term;
    unsigned long pos;
    int c;

    if (out == 0 || out_size == 0UL) {
        return -1;
    }
    out[0] = '\0';
    pos = 0UL;
    have_term = (tcgetattr(0, &old_term) == 0);
    if (have_term) {
        new_term = old_term;
        new_term.c_lflag &= ~(tcflag_t)ECHO;
        (void)tcsetattr(0, TCSANOW, &new_term);
    }
    for (;;) {
        c = getchar();
        if (c == EOF) {
            if (have_term) {
                (void)tcsetattr(0, TCSANOW, &old_term);
            }
            return (pos > 0UL) ? 0 : -1;
        }
        if (c == '\n' || c == '\r') {
            break;
        }
        if (pos + 1UL < out_size) {
            out[pos++] = (char)c;
        }
    }
    out[pos] = '\0';
    if (have_term) {
        (void)tcsetattr(0, TCSANOW, &old_term);
    }
    return 0;
#endif
}

int tg_platform_stdin_set_raw(int enabled)
{
#if defined(__AROS__)
    if (SetMode(Input(), enabled ? 1 : 0)) {
        return 0;
    }
    return -1;
#else
    static struct termios saved;
    static int saved_valid = 0;
    struct termios raw;

    if (enabled) {
        if (tcgetattr(0, &saved) != 0) {
            return -1;
        }
        saved_valid = 1;
        raw = saved;
        raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(0, TCSANOW, &raw) != 0) {
            return -1;
        }
        return 0;
    }
    if (saved_valid) {
        (void)tcsetattr(0, TCSANOW, &saved);
        saved_valid = 0;
    }
    return 0;
#endif
}

/*
 * In-tree CSPRNG fallback for AROS (SHA-256 Hash-DRBG).
 *
 * Real AROS has no /dev/urandom and this MTProto build links no TLS, so the
 * POSIX random path returns nothing and login fails with "secure-rng-
 * unavailable". Mirror the AmigaOS 3/4 backends: gather local entropy (x86 TSC
 * jitter, wall clock, run-varying addresses, a per-call counter) and run it
 * through SHA-256 in a Hash-DRBG. Only used when /dev/urandom is absent, so the
 * macOS host build keeps using the real kernel CSPRNG.
 */
static unsigned char tg_aros_drbg_state[TG_MTPROTO_SHA256_LENGTH];
static int tg_aros_drbg_ready = 0;

static unsigned long tg_aros_timebase(void)
{
    unsigned long tb = 0;
#if defined(__i386__) || defined(__x86_64__)
    unsigned int lo = 0;
    unsigned int hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    tb = (unsigned long)lo ^ ((unsigned long)hi << 13);
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    tb = (unsigned long)tv.tv_usec ^ ((unsigned long)tv.tv_sec << 8);
#endif
    return tb;
}

static unsigned long tg_aros_entropy_gather(unsigned char *buf,
                                            unsigned long cap)
{
    unsigned long n = 0;
    unsigned long i;
    struct timeval tv;
    time_t now;
    void *p;

    now = time(0);
    if (n + sizeof(now) <= cap) {
        memcpy(buf + n, &now, sizeof(now));
        n += sizeof(now);
    }
    for (i = 0; i < 96UL && n + sizeof(unsigned long) <= cap; ++i) {
        unsigned long tb = tg_aros_timebase();
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
    p = (void *)tg_aros_drbg_state;
    if (n + sizeof(p) <= cap) { memcpy(buf + n, &p, sizeof(p)); n += sizeof(p); }
    p = (void *)&n;
    if (n + sizeof(p) <= cap) { memcpy(buf + n, &p, sizeof(p)); n += sizeof(p); }
    return n;
}

static void tg_aros_drbg_generate(unsigned char *out, unsigned long n);

static FILE *tg_aros_open_seed_file(const char *mode)
{
    /* PROGDIR: keeps the seed next to the binary; some C libraries do not
       grok Amiga-style paths, so fall back to the current directory (the
       icon launcher CDs into the drawer anyway). */
    FILE *f = fopen("PROGDIR:telegram-seed.bin", mode);
    if (f == 0) {
        f = fopen("telegram-seed.bin", mode);
    }
    return f;
}

/*
 * Persistent seed (PROGDIR:telegram-seed.bin, Linux random-seed style):
 * entropy accumulates across runs instead of restarting from a cold,
 * reproducible VM boot. The file is mixed into the pool at seed time and
 * immediately overwritten with fresh DRBG output, so yesterday's file
 * never predicts today's state.
 */
static void tg_aros_drbg_seed(void)
{
    unsigned char pool[1024];
    unsigned long len = tg_aros_entropy_gather(pool, sizeof(pool));
    {
        FILE *seed_file = tg_aros_open_seed_file("rb");
        if (seed_file != 0) {
            unsigned char saved[64];
            unsigned long got = (unsigned long)fread(saved, 1U,
                                                     sizeof(saved),
                                                     seed_file);
            fclose(seed_file);
            if (got > 0UL && len + got <= sizeof(pool)) {
                memcpy(pool + len, saved, got);
                len += got;
            }
        }
    }
    tg_mtproto_sha256(pool, len, tg_aros_drbg_state);
    tg_aros_drbg_ready = 1;
    {
        unsigned char fresh[64];
        FILE *seed_file;
        tg_aros_drbg_generate(fresh, sizeof(fresh));
        seed_file = tg_aros_open_seed_file("wb");
        if (seed_file != 0) {
            fwrite(fresh, 1U, sizeof(fresh), seed_file);
            fclose(seed_file);
        }
        memset(fresh, 0, sizeof(fresh));
    }
}

static void tg_aros_drbg_generate(unsigned char *out, unsigned long n)
{
    static unsigned long calls = 0;
    unsigned char key[TG_MTPROTO_SHA256_LENGTH];
    unsigned char block[TG_MTPROTO_SHA256_LENGTH];
    unsigned long off;
    unsigned long ctr;

    if (!tg_aros_drbg_ready) {
        tg_aros_drbg_seed();
    }
    {
        unsigned char work[TG_MTPROTO_SHA256_LENGTH + 32 +
                           sizeof(tg_aros_key_ring) + sizeof(unsigned long)];
        unsigned long wn = TG_MTPROTO_SHA256_LENGTH;
        struct timeval tv;
        unsigned long tb = tg_aros_timebase();
        memcpy(work, tg_aros_drbg_state, TG_MTPROTO_SHA256_LENGTH);
        gettimeofday(&tv, 0);
        ++calls;
        memcpy(work + wn, &tv, sizeof(tv)); wn += sizeof(tv);
        memcpy(work + wn, &tb, sizeof(tb)); wn += sizeof(tb);
        memcpy(work + wn, &calls, sizeof(calls)); wn += sizeof(calls);
        /* Keystroke-timing ring: human input collected since the last
           generate (cheap on the input path, absorbed here). */
        memcpy(work + wn, tg_aros_key_ring, sizeof(tg_aros_key_ring));
        wn += sizeof(tg_aros_key_ring);
        memcpy(work + wn, &tg_aros_key_ring_pos,
               sizeof(tg_aros_key_ring_pos));
        wn += sizeof(tg_aros_key_ring_pos);
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
    {
        unsigned char rb[TG_MTPROTO_SHA256_LENGTH + 1];
        memcpy(rb, key, TG_MTPROTO_SHA256_LENGTH);
        rb[TG_MTPROTO_SHA256_LENGTH] = 0x01;
        tg_mtproto_sha256(rb, sizeof(rb), tg_aros_drbg_state);
    }
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
    /* No /dev/urandom on real AROS: use the in-tree Hash-DRBG. */
    tg_aros_drbg_generate(bytes, byte_count);
    return 1;
}

static void tg_platform_set_error(char *error_buffer, unsigned long error_buffer_size,
                                  const char *message)
{
    if (error_buffer != 0 && error_buffer_size > 0) {
        strncpy(error_buffer, message, error_buffer_size - 1);
        error_buffer[error_buffer_size - 1] = '\0';
    }
}

#if defined(__AROS__)
static int tg_aros_open_socket_library(void)
{
    if (SocketBase != 0) {
        return 1;
    }

    SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 3);
    return SocketBase != 0;
}

static void tg_aros_close_socket_library(void)
{
    if (SocketBase != 0) {
        CloseLibrary(SocketBase);
        SocketBase = 0;
    }
}
#endif

#if !defined(__AROS__)
static tg_net_status tg_platform_connect_socket(int sock, struct sockaddr_in *address,
                                                char *error_buffer,
                                                unsigned long error_buffer_size)
{
    unsigned long timeout_seconds;
    int flags;
    int rc;
    int socket_error;
    socklen_t socket_error_size;
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
#endif

tg_net_status tg_platform_tcp_connect(tg_net_connection *connection, const char *host,
                                      const char *port, char *error_buffer,
                                      unsigned long error_buffer_size)
{
    struct hostent *host_entry;
    struct sockaddr_in address;
    long port_number;
    int rc;
    int sock;

    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    port_number = strtol(port, 0, 10);
    if (port_number <= 0 || port_number > 65535) {
        return TG_NET_INVALID_ARGUMENT;
    }

#if defined(__AROS__)
    if (!tg_aros_open_socket_library()) {
        tg_platform_set_error(error_buffer, error_buffer_size,
                              "cannot open bsdsocket.library");
        return TG_NET_CONNECT_FAILED;
    }
#endif

    host_entry = gethostbyname(host);
    if (host_entry == 0 || host_entry->h_addr_list == 0 ||
        host_entry->h_addr_list[0] == 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, "host lookup failed");
#if defined(__AROS__)
        tg_aros_close_socket_library();
#endif
        return TG_NET_RESOLVE_FAILED;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port_number);
    memcpy(&address.sin_addr, host_entry->h_addr_list[0], sizeof(address.sin_addr));

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
#if defined(__AROS__)
        tg_aros_close_socket_library();
#endif
        return TG_NET_CONNECT_FAILED;
    }

#if defined(__AROS__)
    rc = connect(sock, (struct sockaddr *)&address, sizeof(address));
    if (rc == 0) {
#else
    (void)rc;
    if (tg_platform_connect_socket(sock, &address, error_buffer,
                                   error_buffer_size) == TG_NET_OK) {
#endif
        connection->platform_handle = sock;
        connection->is_open = 1;
        return TG_NET_OK;
    }

    close(sock);
#if defined(__AROS__)
    tg_platform_set_error(error_buffer, error_buffer_size, strerror(errno));
#endif
#if defined(__AROS__)
    tg_aros_close_socket_library();
#endif
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
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    /* Bound the blocking recv() with a select() timeout, mirroring the MorphOS
       backend. Without this the encrypted-query receive loop in
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
        close((int)connection->platform_handle);
        connection->is_open = 0;
    }
#if defined(__AROS__)
    tg_aros_close_socket_library();
#endif
}

#if TG_ENABLE_TLS

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

static tg_tls_status tg_aros_configure_certificate_validation(SSL_CTX *ctx,
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

    OPENSSL_init_ssl(0, 0);

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
    verify_status = tg_aros_configure_certificate_validation(
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
#if defined(__AROS__)
    /* Peek without clearing: the break stays pending for outer loops. */
    return (SetSignal(0L, 0L) & SIGBREAKF_CTRL_C) != 0L;
#else
    return 0;
#endif
}

#if defined(__AROS__)
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase = 0;
#endif

void tg_platform_display_beep(void)
{
#if defined(__AROS__)
    /* The screen flash is the Amiga-native notification; a BEL byte lets
       console handlers improvise (one icon console draws it as a glyph). */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library",
                                                        0L);
    if (IntuitionBase != 0) {
        DisplayBeep(0L);
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = 0;
    }
#endif
}
