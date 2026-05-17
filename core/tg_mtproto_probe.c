/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <time.h>

#include "tg_mtproto_envelope.h"
#include "tg_mtproto_message_id.h"
#include "tg_mtproto_probe.h"
#include "tg_mtproto_transport.h"
#include "tg_net.h"

static void tg_mtproto_probe_nonce(unsigned char nonce[16])
{
    unsigned long seed;
    unsigned int i;

    seed = (unsigned long)time(0);
    for (i = 0; i < 16U; ++i) {
        seed = (seed * 1103515245UL) + 12345UL;
        nonce[i] = (unsigned char)((seed >> 16) & 0xffUL);
    }
}

static tg_net_status tg_mtproto_send_all(tg_net_connection *connection,
                                         const unsigned char *data,
                                         unsigned long length,
                                         char *error_buffer,
                                         unsigned long error_buffer_size)
{
    unsigned long sent;
    unsigned long offset;
    tg_net_status status;

    offset = 0;
    while (offset < length) {
        status = tg_net_send(connection, data + offset, length - offset, &sent,
                             error_buffer, error_buffer_size);
        if (status != TG_NET_OK) {
            return status;
        }
        if (sent == 0) {
            return TG_NET_SEND_FAILED;
        }
        offset += sent;
    }

    return TG_NET_OK;
}

static tg_net_status tg_mtproto_recv_exact(tg_net_connection *connection,
                                           unsigned char *data,
                                           unsigned long length,
                                           char *error_buffer,
                                           unsigned long error_buffer_size)
{
    unsigned long received;
    unsigned long offset;
    tg_net_status status;

    offset = 0;
    while (offset < length) {
        status = tg_net_recv(connection, data + offset, length - offset,
                             &received, error_buffer, error_buffer_size);
        if (status != TG_NET_OK) {
            return status;
        }
        if (received == 0) {
            return TG_NET_CLOSED;
        }
        offset += received;
    }

    return TG_NET_OK;
}

static tg_net_status tg_mtproto_recv_abridged_packet(
    tg_net_connection *connection,
    unsigned char *payload,
    unsigned long payload_capacity,
    unsigned long *payload_length,
    char *error_buffer,
    unsigned long error_buffer_size)
{
    unsigned char length_header[4];
    unsigned long length_words;
    tg_net_status status;

    if (payload_length != 0) {
        *payload_length = 0;
    }
    if (payload == 0 || payload_length == 0) {
        return TG_NET_INVALID_ARGUMENT;
    }

    status = tg_mtproto_recv_exact(connection, length_header, 1,
                                   error_buffer, error_buffer_size);
    if (status != TG_NET_OK) {
        return status;
    }

    if (length_header[0] < 0x7fU) {
        length_words = length_header[0];
    } else {
        status = tg_mtproto_recv_exact(connection, length_header + 1, 3,
                                       error_buffer, error_buffer_size);
        if (status != TG_NET_OK) {
            return status;
        }
        length_words = ((unsigned long)length_header[1]) |
                       (((unsigned long)length_header[2]) << 8) |
                       (((unsigned long)length_header[3]) << 16);
    }

    *payload_length = length_words * 4UL;
    if (*payload_length > payload_capacity) {
        return TG_NET_RECV_FAILED;
    }

    return tg_mtproto_recv_exact(connection, payload, *payload_length,
                                 error_buffer, error_buffer_size);
}

static unsigned long tg_mtproto_read_u32_le(const unsigned char *data)
{
    return ((unsigned long)data[0]) |
           (((unsigned long)data[1]) << 8) |
           (((unsigned long)data[2]) << 16) |
           (((unsigned long)data[3]) << 24);
}

tg_mtproto_tl_status tg_mtproto_build_req_pq_multi(
    tg_mtproto_tl_writer *writer,
    unsigned long message_id_hi,
    unsigned long message_id_lo,
    const unsigned char nonce[16])
{
    unsigned char body[20];
    tg_mtproto_tl_writer body_writer;
    tg_mtproto_tl_status status;

    if (nonce == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }

    tg_mtproto_tl_writer_init(&body_writer, body, sizeof(body));
    status = tg_mtproto_tl_write_u32(&body_writer, 0xbe7e8ef1UL);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    status = tg_mtproto_tl_write_raw(&body_writer, nonce, 16);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }

    return tg_mtproto_write_plain_message(writer, message_id_hi, message_id_lo,
                                          body, body_writer.length);
}

int tg_mtproto_req_pq_probe(const char *host, const char *port, FILE *stream)
{
    unsigned char nonce[16];
    unsigned char payload[64];
    unsigned char packet[80];
    unsigned char response[1024];
    unsigned long payload_length;
    unsigned long response_length;
    unsigned long constructor;
    tg_mtproto_message_id msg_id;
    tg_mtproto_tl_writer writer;
    tg_net_connection connection;
    tg_net_status net_status;
    char error_buffer[160];

    if (host == 0 || port == 0 || stream == 0) {
        return 2;
    }

    tg_mtproto_probe_nonce(nonce);
    tg_mtproto_client_message_id((unsigned long)time(0), 4UL, 0, &msg_id);

    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_build_req_pq_multi(&writer, msg_id.hi, msg_id.lo, nonce) !=
        TG_MTPROTO_TL_OK) {
        fputs("mtproto req_pq probe: packet-build-failed\n", stream);
        return 2;
    }
    payload_length = writer.length;

    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_init(&writer) != TG_MTPROTO_TL_OK ||
        tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
            TG_MTPROTO_TL_OK) {
        fputs("mtproto req_pq probe: transport-build-failed\n", stream);
        return 2;
    }

    error_buffer[0] = '\0';
    net_status = tg_net_connect(&connection, host, port, error_buffer,
                                sizeof(error_buffer));
    if (net_status != TG_NET_OK) {
        fprintf(stream, "mtproto req_pq probe: connect-failed (%s)\n",
                tg_net_status_name(net_status));
        return 2;
    }

    net_status = tg_mtproto_send_all(&connection, packet, writer.length,
                                     error_buffer, sizeof(error_buffer));
    if (net_status == TG_NET_OK) {
        net_status = tg_mtproto_recv_abridged_packet(&connection, response,
                                                     sizeof(response),
                                                     &response_length,
                                                     error_buffer,
                                                     sizeof(error_buffer));
    }
    tg_net_close(&connection);

    if (net_status != TG_NET_OK) {
        fprintf(stream, "mtproto req_pq probe: transport-failed (%s)\n",
                tg_net_status_name(net_status));
        return 2;
    }

    constructor = 0;
    if (response_length >= 24UL) {
        constructor = tg_mtproto_read_u32_le(response + 20);
    }

    fprintf(stream,
            "mtproto req_pq probe: received %lu bytes, constructor 0x%08lx\n",
            response_length, constructor);

    if (constructor != 0x05162463UL) {
        return 2;
    }

    return 0;
}

int tg_mtproto_probe_self_test(void)
{
    static const unsigned char nonce[16] = {
        0x79U, 0xf0U, 0xafU, 0xb5U, 0x02U, 0x52U, 0xe5U, 0xfcU,
        0x96U, 0x92U, 0x4bU, 0xfcU, 0xecU, 0xdaU, 0x4fU, 0x05U
    };
    static const unsigned char expected[] = {
        0xefU, 0x0aU,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x60U, 0x97U, 0x05U, 0x00U, 0xebU, 0xe5U, 0x77U, 0x67U,
        0x14U, 0x00U, 0x00U, 0x00U,
        0xf1U, 0x8eU, 0x7eU, 0xbeU,
        0x79U, 0xf0U, 0xafU, 0xb5U, 0x02U, 0x52U, 0xe5U, 0xfcU,
        0x96U, 0x92U, 0x4bU, 0xfcU, 0xecU, 0xdaU, 0x4fU, 0x05U
    };
    unsigned char payload[64];
    unsigned char packet[80];
    tg_mtproto_tl_writer writer;

    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_build_req_pq_multi(&writer, 0x6777e5ebUL, 0x00059760UL,
                                      nonce) != TG_MTPROTO_TL_OK ||
        writer.length != 40UL) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_init(&writer) != TG_MTPROTO_TL_OK ||
        tg_mtproto_write_abridged_packet(&writer, payload, 40UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != sizeof(expected) ||
        memcmp(packet, expected, sizeof(expected)) != 0) {
        return 2;
    }

    return 0;
}
