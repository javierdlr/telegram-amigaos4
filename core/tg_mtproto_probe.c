/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "tg_mtproto_auth.h"
#include "tg_mtproto_encrypted.h"
#include "tg_mtproto_envelope.h"
#include "tg_mtproto_login.h"
#include "tg_mtproto_message_id.h"
#include "tg_mtproto_probe.h"
#include "tg_mtproto_rsa.h"
#include "tg_mtproto_session.h"
#include "tg_mtproto_transport.h"
#include "tg_file.h"
#include "tg_net.h"
#include "tg_platform.h"

#define TG_MTPROTO_RPC_RESULT_CONSTRUCTOR 0xf35c6d01UL
#define TG_MTPROTO_RPC_ERROR_CONSTRUCTOR 0x2144ca19UL
#define TG_MTPROTO_MSG_CONTAINER_CONSTRUCTOR 0x73f1f8dcUL
#define TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR 0x3072cfa1UL
#define TG_MTPROTO_AUTH_SENT_CODE_CONSTRUCTOR 0x5e002502UL
#define TG_MTPROTO_AUTH_SENT_CODE_SUCCESS_CONSTRUCTOR 0x2390fe44UL
#define TG_MTPROTO_AUTH_SENT_CODE_PAYMENT_REQUIRED_CONSTRUCTOR 0xd7a2fcf9UL
#define TG_MTPROTO_CONFIG_CONSTRUCTOR 0xcc1a241eUL
#define TG_MTPROTO_ACCOUNT_PASSWORD_CONSTRUCTOR 0x957b50fbUL

typedef struct tg_mtproto_auth_context {
    tg_net_connection connection;
    tg_mtproto_session session;
    tg_mtproto_message_id last_msg_id;
    unsigned char auth_key[TG_MTPROTO_AUTH_KEY_LENGTH];
    int connection_open;
} tg_mtproto_auth_context;

static void tg_mtproto_probe_random(unsigned char *bytes, unsigned long length)
{
    static unsigned long seed = 0;
    unsigned long i;

    if (tg_platform_random_bytes(bytes, length)) {
        return;
    }
    if (seed == 0UL) {
        seed = (unsigned long)time(0);
    }
    for (i = 0; i < length; ++i) {
        seed = (seed * 1103515245UL) + 12345UL;
        bytes[i] = (unsigned char)((seed >> 16) & 0xffUL);
    }
}

static int tg_mtproto_secure_random(unsigned char *bytes, unsigned long length)
{
    return tg_platform_random_bytes(bytes, length);
}

static void tg_mtproto_probe_nonce(unsigned char nonce[16])
{
    tg_mtproto_probe_random(nonce, 16UL);
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

static void tg_mtproto_u32_be(unsigned long value, unsigned char bytes[4])
{
    bytes[0] = (unsigned char)((value >> 24) & 0xffUL);
    bytes[1] = (unsigned char)((value >> 16) & 0xffUL);
    bytes[2] = (unsigned char)((value >> 8) & 0xffUL);
    bytes[3] = (unsigned char)(value & 0xffUL);
}

static int tg_mtproto_body_is_expected_pong(const unsigned char *body,
                                            unsigned long body_length,
                                            unsigned long ping_id_hi,
                                            unsigned long ping_id_lo)
{
    return body != 0 && body_length >= 20UL &&
           tg_mtproto_read_u32_le(body) == 0x347773c5UL &&
           tg_mtproto_read_u32_le(body + 12U) == ping_id_lo &&
           tg_mtproto_read_u32_le(body + 16U) == ping_id_hi;
}

static int tg_mtproto_container_has_expected_pong(
    const unsigned char *body,
    unsigned long body_length,
    unsigned long ping_id_hi,
    unsigned long ping_id_lo)
{
    unsigned long count;
    unsigned long index;
    unsigned long offset;
    unsigned long nested_length;

    if (body == 0 || body_length < 8UL ||
        tg_mtproto_read_u32_le(body) != 0x73f1f8dcUL) {
        return 0;
    }
    count = tg_mtproto_read_u32_le(body + 4U);
    offset = 8UL;
    for (index = 0UL; index < count; ++index) {
        if (body_length - offset < 16UL) {
            return 0;
        }
        nested_length = tg_mtproto_read_u32_le(body + offset + 12U);
        offset += 16UL;
        if (nested_length > body_length - offset) {
            return 0;
        }
        if (tg_mtproto_body_is_expected_pong(body + offset, nested_length,
                                             ping_id_hi, ping_id_lo)) {
            return 1;
        }
        offset += nested_length;
    }
    return 0;
}

static int tg_mtproto_parse_dc_id(const char *text, long *dc_id)
{
    char *endptr;
    long value;

    if (text == 0 || text[0] == '\0' || dc_id == 0) {
        return 1;
    }
    value = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || value < -100000L ||
        value > 100000L) {
        return 1;
    }
    *dc_id = value;
    return 0;
}

static int tg_mtproto_parse_ulong_arg(const char *text, unsigned long *out)
{
    char *endptr;
    unsigned long value;

    if (text == 0 || text[0] == '\0' || out == 0) {
        return 1;
    }
    value = strtoul(text, &endptr, 10);
    if (endptr == text || *endptr != '\0') {
        return 1;
    }
    *out = value;
    return 0;
}

static void tg_mtproto_close_auth_context(tg_mtproto_auth_context *context)
{
    if (context != 0 && context->connection_open) {
        tg_net_close(&context->connection);
        context->connection_open = 0;
    }
}

static int tg_mtproto_find_rpc_result_direct(
    const unsigned char *body,
    unsigned long body_length,
    unsigned long request_msg_id_hi,
    unsigned long request_msg_id_lo,
    tg_mtproto_rpc_result *out)
{
    tg_mtproto_rpc_result result;

    if (tg_mtproto_parse_rpc_result(body, body_length, &result) !=
            TG_MTPROTO_TL_OK) {
        return 0;
    }
    if (result.request_msg_id_hi != request_msg_id_hi ||
        result.request_msg_id_lo != request_msg_id_lo) {
        return 0;
    }
    if (out != 0) {
        *out = result;
    }
    return 1;
}

static int tg_mtproto_find_rpc_result(
    const unsigned char *body,
    unsigned long body_length,
    unsigned long request_msg_id_hi,
    unsigned long request_msg_id_lo,
    tg_mtproto_rpc_result *out)
{
    unsigned long count;
    unsigned long index;
    unsigned long offset;
    unsigned long nested_length;

    if (tg_mtproto_find_rpc_result_direct(body, body_length, request_msg_id_hi,
                                          request_msg_id_lo, out)) {
        return 1;
    }
    if (body == 0 || body_length < 8UL ||
        tg_mtproto_read_u32_le(body) != TG_MTPROTO_MSG_CONTAINER_CONSTRUCTOR) {
        return 0;
    }
    count = tg_mtproto_read_u32_le(body + 4U);
    offset = 8UL;
    for (index = 0UL; index < count; ++index) {
        if (body_length - offset < 16UL) {
            return 0;
        }
        nested_length = tg_mtproto_read_u32_le(body + offset + 12U);
        offset += 16UL;
        if (nested_length > body_length - offset) {
            return 0;
        }
        if (tg_mtproto_find_rpc_result_direct(body + offset, nested_length,
                                              request_msg_id_hi,
                                              request_msg_id_lo, out)) {
            return 1;
        }
        offset += nested_length;
    }
    return 0;
}

static int tg_mtproto_find_bad_msg_direct(
    const unsigned char *body,
    unsigned long body_length,
    unsigned long request_msg_id_hi,
    unsigned long request_msg_id_lo,
    tg_mtproto_bad_msg_notification *out)
{
    tg_mtproto_bad_msg_notification notification;

    if (tg_mtproto_parse_bad_msg_notification(body, body_length,
                                              &notification) !=
            TG_MTPROTO_TL_OK) {
        return 0;
    }
    if (notification.bad_msg_id_hi != request_msg_id_hi ||
        notification.bad_msg_id_lo != request_msg_id_lo) {
        return 0;
    }
    if (out != 0) {
        *out = notification;
    }
    return 1;
}

static int tg_mtproto_find_bad_msg(
    const unsigned char *body,
    unsigned long body_length,
    unsigned long request_msg_id_hi,
    unsigned long request_msg_id_lo,
    tg_mtproto_bad_msg_notification *out)
{
    unsigned long count;
    unsigned long index;
    unsigned long offset;
    unsigned long nested_length;

    if (tg_mtproto_find_bad_msg_direct(body, body_length, request_msg_id_hi,
                                       request_msg_id_lo, out)) {
        return 1;
    }
    if (body == 0 || body_length < 8UL ||
        tg_mtproto_read_u32_le(body) != TG_MTPROTO_MSG_CONTAINER_CONSTRUCTOR) {
        return 0;
    }
    count = tg_mtproto_read_u32_le(body + 4U);
    offset = 8UL;
    for (index = 0UL; index < count; ++index) {
        if (body_length - offset < 16UL) {
            return 0;
        }
        nested_length = tg_mtproto_read_u32_le(body + offset + 12U);
        offset += 16UL;
        if (nested_length > body_length - offset) {
            return 0;
        }
        if (tg_mtproto_find_bad_msg_direct(body + offset, nested_length,
                                           request_msg_id_hi,
                                           request_msg_id_lo, out)) {
            return 1;
        }
        offset += nested_length;
    }
    return 0;
}

static void tg_mtproto_collect_ack_ids(
    const tg_mtproto_encrypted_message *message,
    unsigned long *ack_hi,
    unsigned long *ack_lo,
    unsigned long ack_capacity,
    unsigned long *ack_count)
{
    unsigned long count;
    unsigned long index;
    unsigned long offset;
    unsigned long nested_length;

    if (ack_count != 0) {
        *ack_count = 0;
    }
    if (message == 0 || ack_hi == 0 || ack_lo == 0 || ack_count == 0 ||
        ack_capacity == 0UL) {
        return;
    }

    ack_hi[0] = message->message_id_hi;
    ack_lo[0] = message->message_id_lo;
    *ack_count = 1UL;

    if (message->body_length < 8UL ||
        tg_mtproto_read_u32_le(message->body) !=
            TG_MTPROTO_MSG_CONTAINER_CONSTRUCTOR) {
        return;
    }

    count = tg_mtproto_read_u32_le(message->body + 4U);
    offset = 8UL;
    for (index = 0UL; index < count && *ack_count < ack_capacity; ++index) {
        if (message->body_length - offset < 16UL) {
            return;
        }
        ack_lo[*ack_count] = tg_mtproto_read_u32_le(message->body + offset);
        ack_hi[*ack_count] = tg_mtproto_read_u32_le(message->body + offset + 4U);
        nested_length = tg_mtproto_read_u32_le(message->body + offset + 12U);
        offset += 16UL;
        if (nested_length > message->body_length - offset) {
            return;
        }
        ++(*ack_count);
        offset += nested_length;
    }
}

static int tg_mtproto_send_encrypted_service(
    tg_mtproto_auth_context *context,
    const unsigned char *body,
    unsigned long body_length,
    FILE *stream,
    const char *label)
{
    unsigned char encrypted_padding[64];
    unsigned char payload[1024];
    unsigned char packet[1100];
    unsigned long encrypted_padding_length;
    unsigned long payload_length;
    tg_mtproto_message_id msg_id;
    tg_mtproto_tl_writer writer;
    tg_net_status net_status;
    char error_buffer[160];

    if (context == 0 || !context->connection_open || body == 0 ||
        body_length == 0UL || stream == 0 || label == 0) {
        return 2;
    }

    encrypted_padding_length = 12UL;
    while (((32UL + body_length + encrypted_padding_length) % 16UL) != 0UL) {
        ++encrypted_padding_length;
    }
    if (!tg_mtproto_secure_random(encrypted_padding,
                                  encrypted_padding_length)) {
        fprintf(stream, "%s: service-secure-rng-unavailable\n", label);
        return 2;
    }
    tg_mtproto_client_message_id((unsigned long)time(0), 20UL,
                                 &context->last_msg_id, &msg_id);
    context->last_msg_id = msg_id;
    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_write_encrypted_message(
            &writer, context->auth_key, context->session.server_salt_hi,
            context->session.server_salt_lo, context->session.session_id,
            msg_id.hi, msg_id.lo, context->session.seq_no - 1UL,
            body, body_length, encrypted_padding,
            encrypted_padding_length) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: service-build-failed\n", label);
        return 2;
    }
    payload_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: service-transport-build-failed\n", label);
        return 2;
    }
    error_buffer[0] = '\0';
    net_status = tg_mtproto_send_all(&context->connection, packet,
                                     writer.length, error_buffer,
                                     sizeof(error_buffer));
    if (net_status != TG_NET_OK) {
        fprintf(stream, "%s: service-send-failed (%s)\n", label,
                tg_net_status_name(net_status));
        return 2;
    }
    return 0;
}

static void tg_mtproto_ack_server_messages(
    tg_mtproto_auth_context *context,
    const unsigned long *ack_hi,
    const unsigned long *ack_lo,
    unsigned long ack_count,
    FILE *stream,
    const char *label)
{
    unsigned char body[256];
    tg_mtproto_tl_writer writer;

    if (ack_count == 0UL) {
        return;
    }
    tg_mtproto_tl_writer_init(&writer, body, sizeof(body));
    if (tg_mtproto_build_msgs_ack(&writer, ack_hi, ack_lo, ack_count) !=
        TG_MTPROTO_TL_OK) {
        return;
    }
    (void)tg_mtproto_send_encrypted_service(context, body, writer.length,
                                            stream, label);
}

static int tg_mtproto_send_encrypted_query(
    tg_mtproto_auth_context *context,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_rpc_result *rpc_result,
    FILE *stream,
    const char *label)
{
    unsigned char encrypted_padding[64];
    unsigned char payload[3072];
    unsigned char packet[3200];
    unsigned char response[4096];
    unsigned long encrypted_padding_length;
    unsigned long payload_length;
    unsigned long response_length;
    unsigned int attempt;
    unsigned int receive_attempt;
    unsigned long response_constructor;
    unsigned long ack_hi[16];
    unsigned long ack_lo[16];
    unsigned long ack_count;
    tg_mtproto_bad_msg_notification bad_msg;
    tg_mtproto_encrypted_message decrypted;
    tg_mtproto_message_id request_msg_id;
    tg_mtproto_tl_writer writer;
    tg_net_status net_status;
    char error_buffer[160];

    if (context == 0 || !context->connection_open || body == 0 ||
        body_length == 0UL || rpc_result == 0 || stream == 0 ||
        label == 0) {
        return 2;
    }

    for (attempt = 0U; attempt < 2U; ++attempt) {
        encrypted_padding_length = 12UL;
        while (((32UL + body_length + encrypted_padding_length) % 16UL) !=
               0UL) {
            ++encrypted_padding_length;
        }
        if (!tg_mtproto_secure_random(encrypted_padding,
                                      encrypted_padding_length)) {
            fprintf(stream, "%s: secure-rng-unavailable\n", label);
            return 2;
        }
        tg_mtproto_client_message_id((unsigned long)time(0), 16UL,
                                     &context->last_msg_id, &request_msg_id);
        context->last_msg_id = request_msg_id;

        tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
        if (tg_mtproto_write_encrypted_message(
                &writer, context->auth_key, context->session.server_salt_hi,
                context->session.server_salt_lo, context->session.session_id,
                request_msg_id.hi, request_msg_id.lo,
                context->session.seq_no, body, body_length,
                encrypted_padding, encrypted_padding_length) !=
            TG_MTPROTO_TL_OK) {
            fprintf(stream, "%s: encrypted-query-build-failed\n", label);
            return 2;
        }
        payload_length = writer.length;

        tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
        if (tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
            TG_MTPROTO_TL_OK) {
            fprintf(stream, "%s: transport-build-failed\n", label);
            return 2;
        }

        error_buffer[0] = '\0';
        net_status = tg_mtproto_send_all(&context->connection, packet,
                                         writer.length, error_buffer,
                                         sizeof(error_buffer));
        memset(&bad_msg, 0, sizeof(bad_msg));
        for (receive_attempt = 0U; receive_attempt < 3U; ++receive_attempt) {
            if (net_status == TG_NET_OK) {
                net_status = tg_mtproto_recv_abridged_packet(
                    &context->connection, response, sizeof(response),
                    &response_length, error_buffer, sizeof(error_buffer));
            }
            if (net_status != TG_NET_OK) {
                fprintf(stream, "%s: transport-failed (%s)\n", label,
                        tg_net_status_name(net_status));
                return 2;
            }
            if (tg_mtproto_decrypt_encrypted_message(response, response_length,
                                                     context->auth_key,
                                                     &decrypted) !=
                TG_MTPROTO_TL_OK) {
                fprintf(stream, "%s: encrypted-response-decrypt-failed\n",
                        label);
                return 2;
            }
            if (tg_mtproto_find_rpc_result(decrypted.body,
                                           decrypted.body_length,
                                           request_msg_id.hi,
                                           request_msg_id.lo,
                                           rpc_result)) {
                tg_mtproto_collect_ack_ids(&decrypted, ack_hi, ack_lo,
                                           16UL, &ack_count);
                context->session.seq_no += 2UL;
                tg_mtproto_ack_server_messages(context, ack_hi, ack_lo,
                                               ack_count, stream, label);
                return 0;
            }
            if (tg_mtproto_find_bad_msg(decrypted.body, decrypted.body_length,
                                        request_msg_id.hi, request_msg_id.lo,
                                        &bad_msg)) {
                if (bad_msg.has_new_server_salt &&
                    bad_msg.error_code == 48UL) {
                    context->session.server_salt_hi =
                        bad_msg.new_server_salt_hi;
                    context->session.server_salt_lo =
                        bad_msg.new_server_salt_lo;
                    break;
                }
                fprintf(stream, "%s: bad-msg error-code %lu\n", label,
                        bad_msg.error_code);
                return 2;
            }
            response_constructor = decrypted.body_length >= 4UL ?
                tg_mtproto_read_u32_le(decrypted.body) : 0UL;
            if (response_constructor != TG_MTPROTO_MSG_CONTAINER_CONSTRUCTOR &&
                response_constructor != 0x9ec20908UL) {
                fprintf(stream,
                        "%s: encrypted-response-unexpected constructor 0x%08lx\n",
                        label, response_constructor);
                return 2;
            }
        }
        if (bad_msg.has_new_server_salt && bad_msg.error_code == 48UL) {
            continue;
        }
        fprintf(stream, "%s: rpc-response-not-received\n", label);
        return 2;
    }

    fprintf(stream, "%s: bad-server-salt-retry-failed\n", label);
    return 2;
}

static int tg_mtproto_open_auth_context(const char *host,
                                        const char *port,
                                        const char *dc_id_text,
                                        tg_mtproto_auth_context *context,
                                        FILE *stream,
                                        const char *label)
{
    unsigned char nonce[16];
    unsigned char new_nonce[32];
    unsigned char padding[96];
    unsigned char temp_key[32];
    unsigned char p_bytes[4];
    unsigned char q_bytes[4];
    unsigned char inner_data[160];
    unsigned char encrypted_data[TG_MTPROTO_RSA_PADDED_LENGTH];
    unsigned char client_encrypted[TG_MTPROTO_DH_ENCRYPTED_ANSWER_MAX];
    unsigned char b[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char client_padding[15];
    unsigned char session_id[8];
    unsigned char body[384];
    unsigned char payload[512];
    unsigned char packet[600];
    unsigned char response[1200];
    unsigned long body_length;
    unsigned long client_encrypted_length;
    unsigned long payload_length;
    unsigned long response_length;
    unsigned long constructor;
    unsigned long p;
    unsigned long q;
    unsigned int i;
    long dc_id;
    tg_mtproto_message_id first_msg_id;
    tg_mtproto_message_id second_msg_id;
    tg_mtproto_message_id third_msg_id;
    tg_mtproto_res_pq res_pq;
    tg_mtproto_server_dh_params_ok params_ok;
    tg_mtproto_server_dh_inner_data inner;
    tg_mtproto_set_client_dh_answer dh_answer;
    tg_mtproto_tl_writer writer;
    tg_net_status net_status;
    const tg_mtproto_public_key *public_key;
    char error_buffer[160];

    if (host == 0 || port == 0 || context == 0 || stream == 0 ||
        label == 0 || tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0) {
        fprintf(stream, "%s: invalid-arguments\n", label);
        return 2;
    }
    memset(context, 0, sizeof(*context));

    if (!tg_mtproto_secure_random(nonce, sizeof(nonce)) ||
        !tg_mtproto_secure_random(new_nonce, sizeof(new_nonce)) ||
        !tg_mtproto_secure_random(padding, sizeof(padding)) ||
        !tg_mtproto_secure_random(temp_key, sizeof(temp_key)) ||
        !tg_mtproto_secure_random(b, sizeof(b)) ||
        !tg_mtproto_secure_random(client_padding, sizeof(client_padding)) ||
        !tg_mtproto_secure_random(session_id, sizeof(session_id))) {
        fprintf(stream, "%s: secure-rng-unavailable\n", label);
        return 2;
    }

    tg_mtproto_client_message_id((unsigned long)time(0), 4UL, 0,
                                 &first_msg_id);
    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_build_req_pq_multi(&writer, first_msg_id.hi,
                                      first_msg_id.lo, nonce) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: req-pq-build-failed\n", label);
        return 2;
    }
    payload_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_init(&writer) != TG_MTPROTO_TL_OK ||
        tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
            TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: req-pq-transport-build-failed\n", label);
        return 2;
    }

    error_buffer[0] = '\0';
    net_status = tg_net_connect(&context->connection, host, port, error_buffer,
                                sizeof(error_buffer));
    if (net_status != TG_NET_OK) {
        fprintf(stream, "%s: connect-failed (%s)\n", label,
                tg_net_status_name(net_status));
        return 2;
    }
    context->connection_open = 1;

    net_status = tg_mtproto_send_all(&context->connection, packet,
                                     writer.length, error_buffer,
                                     sizeof(error_buffer));
    if (net_status == TG_NET_OK) {
        net_status = tg_mtproto_recv_abridged_packet(
            &context->connection, response, sizeof(response), &response_length,
            error_buffer, sizeof(error_buffer));
    }
    if (net_status != TG_NET_OK) {
        fprintf(stream, "%s: req-pq-failed (%s)\n", label,
                tg_net_status_name(net_status));
        tg_mtproto_close_auth_context(context);
        return 2;
    }

    constructor = response_length >= 24UL ?
        tg_mtproto_read_u32_le(response + 20) : 0UL;
    if (constructor != 0x05162463UL ||
        tg_mtproto_parse_res_pq(response, response_length, &res_pq) !=
            TG_MTPROTO_TL_OK ||
        !tg_mtproto_res_pq_nonce_matches(&res_pq, nonce) ||
        tg_mtproto_pq_factor(res_pq.pq, res_pq.pq_length, &p, &q) != 0) {
        fprintf(stream, "%s: res-pq-parse-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }

    public_key = tg_mtproto_select_public_key(&res_pq);
    if (public_key == 0) {
        fprintf(stream, "%s: rsa-key-not-found\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }

    tg_mtproto_u32_be(p, p_bytes);
    tg_mtproto_u32_be(q, q_bytes);
    tg_mtproto_tl_writer_init(&writer, inner_data, sizeof(inner_data));
    if (tg_mtproto_build_p_q_inner_data_dc(&writer, res_pq.pq,
                                           res_pq.pq_length, p_bytes,
                                           sizeof(p_bytes), q_bytes,
                                           sizeof(q_bytes), nonce,
                                           res_pq.server_nonce, new_nonce,
                                           dc_id) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: inner-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }

    for (i = 0U; i < 32U; ++i) {
        if (tg_mtproto_rsa_pad(inner_data, writer.length, padding, temp_key,
                               public_key, encrypted_data) ==
            TG_MTPROTO_TL_OK) {
            break;
        }
        if (!tg_mtproto_secure_random(temp_key, sizeof(temp_key))) {
            fprintf(stream, "%s: secure-rng-unavailable\n", label);
            tg_mtproto_close_auth_context(context);
            return 2;
        }
    }
    if (i == 32U) {
        fprintf(stream, "%s: rsa-pad-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, body, sizeof(body));
    if (tg_mtproto_build_req_dh_params(&writer, nonce, res_pq.server_nonce,
                                       p_bytes, sizeof(p_bytes), q_bytes,
                                       sizeof(q_bytes),
                                       &public_key->fingerprint,
                                       encrypted_data) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: req-dh-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    body_length = writer.length;
    tg_mtproto_client_message_id((unsigned long)time(0), 8UL, &first_msg_id,
                                 &second_msg_id);
    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_write_plain_message(&writer, second_msg_id.hi,
                                       second_msg_id.lo, body,
                                       body_length) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: req-dh-envelope-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    payload_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: req-dh-transport-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }

    net_status = tg_mtproto_send_all(&context->connection, packet,
                                     writer.length, error_buffer,
                                     sizeof(error_buffer));
    if (net_status == TG_NET_OK) {
        net_status = tg_mtproto_recv_abridged_packet(
            &context->connection, response, sizeof(response), &response_length,
            error_buffer, sizeof(error_buffer));
    }
    if (net_status != TG_NET_OK) {
        fprintf(stream, "%s: req-dh-failed (%s)\n", label,
                tg_net_status_name(net_status));
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    constructor = response_length >= 24UL ?
        tg_mtproto_read_u32_le(response + 20) : 0UL;
    if (constructor != 0xd0e8075cUL ||
        tg_mtproto_parse_server_dh_params_ok(response, response_length,
                                             &params_ok) != TG_MTPROTO_TL_OK ||
        memcmp(params_ok.nonce, nonce, 16U) != 0 ||
        memcmp(params_ok.server_nonce, res_pq.server_nonce, 16U) != 0 ||
        tg_mtproto_decrypt_server_dh_inner_data(
            params_ok.encrypted_answer, params_ok.encrypted_answer_length,
            new_nonce, nonce, res_pq.server_nonce, &inner) !=
            TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: server-dh-parse-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    if (!tg_mtproto_check_dh_params(&inner)) {
        fprintf(stream, "%s: dh-params-check-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }

    if (tg_mtproto_build_client_dh_request(&inner, new_nonce, b,
                                           client_padding, client_encrypted,
                                           &client_encrypted_length,
                                           context->auth_key) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: client-dh-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, body, sizeof(body));
    if (tg_mtproto_build_set_client_dh_params(
            &writer, nonce, res_pq.server_nonce, client_encrypted,
            client_encrypted_length) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: set-client-dh-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    body_length = writer.length;
    tg_mtproto_client_message_id((unsigned long)time(0), 12UL,
                                 &second_msg_id, &third_msg_id);
    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_write_plain_message(&writer, third_msg_id.hi,
                                       third_msg_id.lo, body,
                                       body_length) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: set-client-envelope-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    payload_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: set-client-transport-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    net_status = tg_mtproto_send_all(&context->connection, packet,
                                     writer.length, error_buffer,
                                     sizeof(error_buffer));
    if (net_status == TG_NET_OK) {
        net_status = tg_mtproto_recv_abridged_packet(
            &context->connection, response, sizeof(response), &response_length,
            error_buffer, sizeof(error_buffer));
    }
    if (net_status != TG_NET_OK) {
        fprintf(stream, "%s: set-client-dh-failed (%s)\n", label,
                tg_net_status_name(net_status));
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    if (tg_mtproto_parse_set_client_dh_answer(response, response_length,
                                              &dh_answer) !=
        TG_MTPROTO_TL_OK ||
        !tg_mtproto_verify_dh_gen_ok(&dh_answer, nonce, res_pq.server_nonce,
                                     new_nonce, context->auth_key)) {
        fprintf(stream, "%s: dh-gen-not-ok\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }

    tg_mtproto_session_from_auth_key(&context->session, (unsigned long)dc_id,
                                     context->auth_key, new_nonce,
                                     res_pq.server_nonce,
                                     session_id);
    context->session.seq_no = 1UL;
    context->last_msg_id = third_msg_id;
    return 0;
}

static int tg_mtproto_load_auth_context(const char *host,
                                        const char *port,
                                        const char *auth_file,
                                        tg_mtproto_auth_context *context,
                                        FILE *stream,
                                        const char *label)
{
    tg_mtproto_session_status session_status;
    tg_net_status net_status;
    tg_mtproto_tl_writer writer;
    unsigned char init_packet[1];
    char error_buffer[160];

    if (host == 0 || port == 0 || auth_file == 0 || context == 0 ||
        stream == 0 || label == 0) {
        fprintf(stream, "%s: invalid-arguments\n", label);
        return 2;
    }
    memset(context, 0, sizeof(*context));
    session_status = tg_mtproto_session_load_authorization(
        auth_file, &context->session, context->auth_key);
    if (session_status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file-load-failed (%s)\n", label,
                tg_mtproto_session_status_name(session_status));
        return 2;
    }

    tg_mtproto_client_message_id((unsigned long)time(0), 4UL, 0,
                                 &context->last_msg_id);
    error_buffer[0] = '\0';
    net_status = tg_net_connect(&context->connection, host, port, error_buffer,
                                sizeof(error_buffer));
    if (net_status != TG_NET_OK) {
        fprintf(stream, "%s: connect-failed (%s)\n", label,
                tg_net_status_name(net_status));
        return 2;
    }
    context->connection_open = 1;
    tg_mtproto_tl_writer_init(&writer, init_packet, sizeof(init_packet));
    if (tg_mtproto_write_abridged_init(&writer) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: transport-init-build-failed\n", label);
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    net_status = tg_mtproto_send_all(&context->connection, init_packet,
                                     writer.length, error_buffer,
                                     sizeof(error_buffer));
    if (net_status != TG_NET_OK) {
        fprintf(stream, "%s: transport-init-failed (%s)\n", label,
                tg_net_status_name(net_status));
        tg_mtproto_close_auth_context(context);
        return 2;
    }
    return 0;
}

static void tg_mtproto_trim_line(char *text)
{
    unsigned long length;

    if (text == 0) {
        return;
    }
    length = (unsigned long)strlen(text);
    while (length > 0UL &&
           (text[length - 1U] == '\n' || text[length - 1U] == '\r' ||
            text[length - 1U] == ' ' || text[length - 1U] == '\t')) {
        text[length - 1U] = '\0';
        --length;
    }
}

static int tg_mtproto_print_rpc_error(const char *label,
                                      const tg_mtproto_rpc_result *result,
                                      FILE *stream)
{
    char error_message[128];
    long error_code;

    if (result == 0 || stream == 0 || label == 0 ||
        result->result_constructor != TG_MTPROTO_RPC_ERROR_CONSTRUCTOR ||
        tg_mtproto_parse_rpc_error(result->result_body - 4U,
                                   result->result_body_length + 4U,
                                   &error_code, error_message,
                                   sizeof(error_message)) !=
            TG_MTPROTO_TL_OK) {
        return 0;
    }
    if (strcmp(error_message, "SESSION_PASSWORD_NEEDED") == 0 ||
        strcmp(error_message, "PHONE_PASSWORD_PROTECTED") == 0) {
        fprintf(stream, "%s: two-factor-password-required\n", label);
    } else {
        fprintf(stream, "%s: rpc-error %ld %s\n", label, error_code,
                error_message);
    }
    return 1;
}

static int tg_mtproto_build_initialized_query(tg_mtproto_tl_writer *writer,
                                              unsigned char *wrapped_query,
                                              unsigned long wrapped_capacity,
                                              unsigned long api_id,
                                              const unsigned char *query,
                                              unsigned long query_length)
{
    unsigned char initialized_query[640];
    tg_mtproto_tl_status status;

    tg_mtproto_tl_writer_init(writer, initialized_query,
                              sizeof(initialized_query));
    status = tg_mtproto_build_init_connection(writer, api_id, "Amiga",
                                              "portable", "0.1", "en",
                                              query, query_length);
    if (status != TG_MTPROTO_TL_OK) {
        return 1;
    }
    query_length = writer->length;
    tg_mtproto_tl_writer_init(writer, wrapped_query, wrapped_capacity);
    status = tg_mtproto_build_invoke_with_layer(writer, 214UL,
                                                initialized_query,
                                                query_length);
    return status == TG_MTPROTO_TL_OK ? 0 : 1;
}

int tg_mtproto_auth_send_code(const char *host,
                              const char *port,
                              const char *dc_id_text,
                              const char *api_id_text,
                              const char *api_hash,
                              const char *phone_number,
                              const char *auth_file,
                              const char *code_hash_file,
                              FILE *stream)
{
    unsigned char query[512];
    unsigned char initialized_query[640];
    unsigned char wrapped_query[760];
    unsigned long api_id;
    unsigned long query_length;
    tg_file_status file_status;
    tg_mtproto_auth_context context;
    tg_mtproto_rpc_result result;
    tg_mtproto_sent_code sent_code;
    tg_mtproto_session_status session_status;
    tg_mtproto_tl_writer writer;
    static const char label[] = "mtproto auth.sendCode";

    if (stream == 0 || host == 0 || port == 0 || dc_id_text == 0 ||
        api_id_text == 0 || api_hash == 0 || phone_number == 0 ||
        auth_file == 0 || code_hash_file == 0 ||
        tg_mtproto_parse_ulong_arg(api_id_text, &api_id) != 0) {
        if (stream != 0) {
            fputs("mtproto auth.sendCode: invalid-arguments\n", stream);
        }
        return 2;
    }

    if (tg_mtproto_open_auth_context(host, port, dc_id_text, &context, stream,
                                     label) != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_send_code(&writer, phone_number, api_id,
                                        api_hash) != TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    query_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, initialized_query,
                              sizeof(initialized_query));
    if (tg_mtproto_build_init_connection(&writer, api_id, "Amiga",
                                         "portable", "0.1", "en", query,
                                         query_length) != TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: init-connection-build-failed\n", label);
        return 2;
    }
    query_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, wrapped_query, sizeof(wrapped_query));
    if (tg_mtproto_build_invoke_with_layer(&writer, 214UL, initialized_query,
                                           query_length) !=
        TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: invoke-layer-build-failed\n", label);
        return 2;
    }

    if (tg_mtproto_send_encrypted_query(&context, wrapped_query, writer.length,
                                        &result, stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        return 2;
    }
    tg_mtproto_close_auth_context(&context);

    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR) {
        fprintf(stream, "%s: gzip-packed-response-unsupported\n", label);
        return 2;
    }
    if (result.result_constructor != TG_MTPROTO_AUTH_SENT_CODE_CONSTRUCTOR &&
        result.result_constructor !=
            TG_MTPROTO_AUTH_SENT_CODE_PAYMENT_REQUIRED_CONSTRUCTOR &&
        result.result_constructor !=
            TG_MTPROTO_AUTH_SENT_CODE_SUCCESS_CONSTRUCTOR) {
        fprintf(stream, "%s: unexpected-result 0x%08lx\n", label,
                result.result_constructor);
        return 2;
    }
    if (tg_mtproto_parse_auth_sent_code(result.result_constructor,
                                        result.result_body,
                                        result.result_body_length,
                                        &sent_code) != TG_MTPROTO_TL_OK ||
        sent_code.phone_code_hash[0] == '\0') {
        fprintf(stream, "%s: sent-code-parse-failed\n", label);
        return 2;
    }

    session_status = tg_mtproto_session_save_authorization(
        auth_file, &context.session, context.auth_key, 1);
    if (session_status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file-save-failed (%s)\n", label,
                tg_mtproto_session_status_name(session_status));
        return 2;
    }
    file_status = tg_file_write_text(code_hash_file, sent_code.phone_code_hash,
                                     (unsigned long)strlen(
                                         sent_code.phone_code_hash));
    if (file_status == TG_FILE_OK) {
        file_status = tg_file_append_text(code_hash_file, "\n", 1UL);
    }
    if (file_status != TG_FILE_OK) {
        fprintf(stream, "%s: code-hash-save-failed (%s)\n", label,
                tg_file_status_name(file_status));
        return 2;
    }

    fprintf(stream, "%s: code sent\n", label);
    fprintf(stream, "%s: auth state saved\n", label);
    fprintf(stream, "%s: phone_code_hash saved\n", label);
    return 0;
}

int tg_mtproto_auth_sign_in(const char *host,
                            const char *port,
                            const char *api_id_text,
                            const char *auth_file,
                            const char *phone_number,
                            const char *code_hash_file,
                            const char *phone_code,
                            const char *dc_id_text,
                            FILE *stream)
{
    unsigned char query[512];
    unsigned char initialized_query[640];
    unsigned char wrapped_query[760];
    char code_hash[160];
    unsigned long code_hash_length;
    unsigned long api_id;
    unsigned long query_length;
    tg_file_status file_status;
    tg_mtproto_auth_context context;
    tg_mtproto_rpc_result result;
    tg_mtproto_session_status session_status;
    tg_mtproto_tl_writer writer;
    long dc_id;
    static const char label[] = "mtproto auth.signIn";

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 ||
        phone_number == 0 || code_hash_file == 0 || phone_code == 0 ||
        tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0 ||
        tg_mtproto_parse_ulong_arg(api_id_text, &api_id) != 0) {
        if (stream != 0) {
            fputs("mtproto auth.signIn: invalid-arguments\n", stream);
        }
        return 2;
    }

    file_status = tg_file_read_text(code_hash_file, code_hash,
                                    sizeof(code_hash), &code_hash_length);
    if (file_status != TG_FILE_OK) {
        fprintf(stream, "%s: code-hash-load-failed (%s)\n", label,
                tg_file_status_name(file_status));
        return 2;
    }
    tg_mtproto_trim_line(code_hash);
    if (code_hash[0] == '\0') {
        fprintf(stream, "%s: code-hash-empty\n", label);
        return 2;
    }

    if (tg_mtproto_load_auth_context(host, port, auth_file, &context, stream,
                                     label) != 0) {
        return 2;
    }
    context.session.dc_id = (unsigned long)dc_id;

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_sign_in(&writer, phone_number, code_hash,
                                      phone_code) != TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    query_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, initialized_query,
                              sizeof(initialized_query));
    if (tg_mtproto_build_init_connection(&writer, api_id, "Amiga",
                                         "portable", "0.1", "en", query,
                                         query_length) != TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: init-connection-build-failed\n", label);
        return 2;
    }
    query_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, wrapped_query, sizeof(wrapped_query));
    if (tg_mtproto_build_invoke_with_layer(&writer, 214UL, initialized_query,
                                           query_length) !=
        TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: invoke-layer-build-failed\n", label);
        return 2;
    }

    if (tg_mtproto_send_encrypted_query(&context, wrapped_query, writer.length,
                                        &result, stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        return 2;
    }
    tg_mtproto_close_auth_context(&context);

    session_status = tg_mtproto_session_save_authorization(
        auth_file, &context.session, context.auth_key, 1);
    if (session_status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file-save-failed (%s)\n", label,
                tg_mtproto_session_status_name(session_status));
        return 2;
    }

    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR) {
        fprintf(stream, "%s: gzip-packed-response-unsupported\n", label);
        return 2;
    }
    if (!tg_mtproto_is_auth_authorization_constructor(
            result.result_constructor)) {
        fprintf(stream, "%s: unexpected-result 0x%08lx\n", label,
                result.result_constructor);
        return 2;
    }
    if (result.result_constructor ==
            0x44747e9aUL) {
        fprintf(stream, "%s: signup-required; run --mtproto-auth-sign-up\n",
                label);
        return 2;
    }

    fprintf(stream, "%s: signed in\n", label);
    fprintf(stream, "%s: auth state updated\n", label);
    return 0;
}

int tg_mtproto_auth_sign_up(const char *host,
                            const char *port,
                            const char *api_id_text,
                            const char *auth_file,
                            const char *phone_number,
                            const char *code_hash_file,
                            const char *first_name,
                            const char *last_name,
                            const char *dc_id_text,
                            FILE *stream)
{
    unsigned char query[512];
    unsigned char initialized_query[640];
    unsigned char wrapped_query[760];
    char code_hash[160];
    unsigned long code_hash_length;
    unsigned long api_id;
    unsigned long query_length;
    tg_file_status file_status;
    tg_mtproto_auth_context context;
    tg_mtproto_rpc_result result;
    tg_mtproto_session_status session_status;
    tg_mtproto_tl_writer writer;
    long dc_id;
    static const char label[] = "mtproto auth.signUp";

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 || phone_number == 0 || code_hash_file == 0 ||
        first_name == 0 || last_name == 0 ||
        tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0 ||
        tg_mtproto_parse_ulong_arg(api_id_text, &api_id) != 0) {
        if (stream != 0) {
            fputs("mtproto auth.signUp: invalid-arguments\n", stream);
        }
        return 2;
    }

    file_status = tg_file_read_text(code_hash_file, code_hash,
                                    sizeof(code_hash), &code_hash_length);
    if (file_status != TG_FILE_OK) {
        fprintf(stream, "%s: code-hash-load-failed (%s)\n", label,
                tg_file_status_name(file_status));
        return 2;
    }
    tg_mtproto_trim_line(code_hash);
    if (code_hash[0] == '\0') {
        fprintf(stream, "%s: code-hash-empty\n", label);
        return 2;
    }

    if (tg_mtproto_load_auth_context(host, port, auth_file, &context, stream,
                                     label) != 0) {
        return 2;
    }
    context.session.dc_id = (unsigned long)dc_id;

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_sign_up(&writer, phone_number, code_hash,
                                      first_name, last_name) !=
        TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    query_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, initialized_query,
                              sizeof(initialized_query));
    if (tg_mtproto_build_init_connection(&writer, api_id, "Amiga",
                                         "portable", "0.1", "en", query,
                                         query_length) != TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: init-connection-build-failed\n", label);
        return 2;
    }
    query_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, wrapped_query, sizeof(wrapped_query));
    if (tg_mtproto_build_invoke_with_layer(&writer, 214UL, initialized_query,
                                           query_length) !=
        TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: invoke-layer-build-failed\n", label);
        return 2;
    }

    if (tg_mtproto_send_encrypted_query(&context, wrapped_query, writer.length,
                                        &result, stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        return 2;
    }
    tg_mtproto_close_auth_context(&context);

    session_status = tg_mtproto_session_save_authorization(
        auth_file, &context.session, context.auth_key, 1);
    if (session_status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file-save-failed (%s)\n", label,
                tg_mtproto_session_status_name(session_status));
        return 2;
    }

    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR) {
        fprintf(stream, "%s: gzip-packed-response-unsupported\n", label);
        return 2;
    }
    if (!tg_mtproto_is_auth_authorization_constructor(
            result.result_constructor)) {
        fprintf(stream, "%s: unexpected-result 0x%08lx\n", label,
                result.result_constructor);
        return 2;
    }
    if (result.result_constructor == 0x44747e9aUL) {
        fprintf(stream, "%s: signup-still-required\n", label);
        return 2;
    }

    fprintf(stream, "%s: signed up\n", label);
    fprintf(stream, "%s: auth state updated\n", label);
    return 0;
}

int tg_mtproto_auth_get_config(const char *host,
                               const char *port,
                               const char *api_id_text,
                               const char *auth_file,
                               const char *dc_id_text,
                               FILE *stream)
{
    unsigned char query[32];
    unsigned char wrapped_query[760];
    unsigned long api_id;
    tg_mtproto_auth_context context;
    tg_mtproto_config_summary config;
    tg_mtproto_rpc_result result;
    tg_mtproto_session_status session_status;
    tg_mtproto_tl_writer writer;
    long dc_id;
    static const char label[] = "mtproto help.getConfig";

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 || tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0 ||
        tg_mtproto_parse_ulong_arg(api_id_text, &api_id) != 0) {
        if (stream != 0) {
            fputs("mtproto help.getConfig: invalid-arguments\n", stream);
        }
        return 2;
    }
    if (tg_mtproto_load_auth_context(host, port, auth_file, &context, stream,
                                     label) != 0) {
        return 2;
    }
    context.session.dc_id = (unsigned long)dc_id;

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_help_get_config(&writer) != TG_MTPROTO_TL_OK ||
        tg_mtproto_build_initialized_query(&writer, wrapped_query,
                                           sizeof(wrapped_query), api_id,
                                           query, 4UL) != 0) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_encrypted_query(&context, wrapped_query, writer.length,
                                        &result, stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        return 2;
    }
    tg_mtproto_close_auth_context(&context);

    session_status = tg_mtproto_session_save_authorization(
        auth_file, &context.session, context.auth_key, 1);
    if (session_status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file-save-failed (%s)\n", label,
                tg_mtproto_session_status_name(session_status));
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR) {
        fprintf(stream, "%s: gzip-packed-response-unsupported\n", label);
        return 2;
    }
    if (tg_mtproto_parse_config_summary(result.result_constructor,
                                        result.result_body,
                                        result.result_body_length,
                                        &config) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: config-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: this_dc %lu\n", label, config.this_dc);
    fprintf(stream, "%s: date %lu expires %lu\n", label, config.date,
            config.expires);
    return 0;
}

int tg_mtproto_auth_get_password(const char *host,
                                 const char *port,
                                 const char *api_id_text,
                                 const char *auth_file,
                                 const char *dc_id_text,
                                 FILE *stream)
{
    unsigned char query[32];
    unsigned char wrapped_query[760];
    unsigned long api_id;
    tg_mtproto_auth_context context;
    tg_mtproto_password_summary password;
    tg_mtproto_rpc_result result;
    tg_mtproto_session_status session_status;
    tg_mtproto_tl_writer writer;
    long dc_id;
    static const char label[] = "mtproto account.getPassword";

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 || tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0 ||
        tg_mtproto_parse_ulong_arg(api_id_text, &api_id) != 0) {
        if (stream != 0) {
            fputs("mtproto account.getPassword: invalid-arguments\n", stream);
        }
        return 2;
    }
    if (tg_mtproto_load_auth_context(host, port, auth_file, &context, stream,
                                     label) != 0) {
        return 2;
    }
    context.session.dc_id = (unsigned long)dc_id;

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_account_get_password(&writer) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_build_initialized_query(&writer, wrapped_query,
                                           sizeof(wrapped_query), api_id,
                                           query, 4UL) != 0) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_encrypted_query(&context, wrapped_query, writer.length,
                                        &result, stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        return 2;
    }
    tg_mtproto_close_auth_context(&context);

    session_status = tg_mtproto_session_save_authorization(
        auth_file, &context.session, context.auth_key, 1);
    if (session_status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file-save-failed (%s)\n", label,
                tg_mtproto_session_status_name(session_status));
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR) {
        fprintf(stream, "%s: gzip-packed-response-unsupported\n", label);
        return 2;
    }
    if (tg_mtproto_parse_account_password_summary(result.result_constructor,
                                                  result.result_body,
                                                  result.result_body_length,
                                                  &password) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: password-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: has_password %s\n", label,
            password.has_password ? "yes" : "no");
    fprintf(stream, "%s: srp_check unsupported\n", label);
    return 0;
}

int tg_mtproto_auth_get_self(const char *host,
                             const char *port,
                             const char *api_id_text,
                             const char *auth_file,
                             const char *dc_id_text,
                             FILE *stream)
{
    unsigned char query[64];
    unsigned char wrapped_query[760];
    unsigned long api_id;
    unsigned long query_length;
    tg_mtproto_auth_context context;
    tg_mtproto_rpc_result result;
    tg_mtproto_session_status session_status;
    tg_mtproto_tl_writer writer;
    tg_mtproto_user_summary user;
    long dc_id;
    static const char label[] = "mtproto users.getUsers(self)";

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 || tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0 ||
        tg_mtproto_parse_ulong_arg(api_id_text, &api_id) != 0) {
        if (stream != 0) {
            fputs("mtproto users.getUsers(self): invalid-arguments\n", stream);
        }
        return 2;
    }
    if (tg_mtproto_load_auth_context(host, port, auth_file, &context, stream,
                                     label) != 0) {
        return 2;
    }
    context.session.dc_id = (unsigned long)dc_id;

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_users_get_self(&writer) != TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    query_length = writer.length;
    if (tg_mtproto_build_initialized_query(&writer, wrapped_query,
                                           sizeof(wrapped_query), api_id,
                                           query, query_length) != 0) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: init-connection-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_encrypted_query(&context, wrapped_query, writer.length,
                                        &result, stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        return 2;
    }
    tg_mtproto_close_auth_context(&context);

    session_status = tg_mtproto_session_save_authorization(
        auth_file, &context.session, context.auth_key, 1);
    if (session_status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file-save-failed (%s)\n", label,
                tg_mtproto_session_status_name(session_status));
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR) {
        fprintf(stream, "%s: gzip-packed-response-unsupported\n", label);
        return 2;
    }
    if (tg_mtproto_parse_user_vector_first(result.result_constructor,
                                           result.result_body,
                                           result.result_body_length,
                                           &user) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: user-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: id 0x%08lx%08lx\n", label, user.id_hi, user.id_lo);
    fprintf(stream, "%s: self %s bot %s\n", label,
            user.is_self ? "yes" : "no", user.is_bot ? "yes" : "no");
    if (user.first_name[0] != '\0' || user.last_name[0] != '\0') {
        fprintf(stream, "%s: name %s %s\n", label, user.first_name,
                user.last_name);
    }
    if (user.username[0] != '\0') {
        fprintf(stream, "%s: username %s\n", label, user.username);
    }
    return 0;
}

int tg_mtproto_auth_forget(const char *auth_file,
                           const char *code_hash_file,
                           FILE *stream)
{
    int removed;

    if (stream == 0 || auth_file == 0 || auth_file[0] == '\0') {
        if (stream != 0) {
            fputs("mtproto auth.forget: invalid-arguments\n", stream);
        }
        return 2;
    }
    removed = 0;
    if (remove(auth_file) == 0) {
        ++removed;
    }
    if (code_hash_file != 0 && code_hash_file[0] != '\0' &&
        remove(code_hash_file) == 0) {
        ++removed;
    }
    fprintf(stream, "mtproto auth.forget: removed %d file(s)\n", removed);
    return 0;
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
    unsigned long p;
    unsigned long q;
    tg_mtproto_message_id msg_id;
    tg_mtproto_res_pq res_pq;
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
    if (tg_mtproto_parse_res_pq(response, response_length, &res_pq) !=
            TG_MTPROTO_TL_OK ||
        !tg_mtproto_res_pq_nonce_matches(&res_pq, nonce)) {
        fputs("mtproto req_pq probe: resPQ-parse-failed\n", stream);
        return 2;
    }

    fprintf(stream,
            "mtproto req_pq probe: pq-bytes %lu, fingerprints %u\n",
            res_pq.pq_length, res_pq.fingerprint_count);
    if (res_pq.fingerprint_count > 0U) {
        fprintf(stream,
                "mtproto req_pq probe: first-fingerprint 0x%08lx%08lx\n",
                res_pq.fingerprints[0].hi, res_pq.fingerprints[0].lo);
    }
    if (tg_mtproto_pq_factor(res_pq.pq, res_pq.pq_length, &p, &q) != 0) {
        fputs("mtproto req_pq probe: pq-factor-failed\n", stream);
        return 2;
    }
    fprintf(stream, "mtproto req_pq probe: p %lu q %lu\n", p, q);

    return 0;
}

int tg_mtproto_req_dh_probe(const char *host, const char *port,
                            const char *dc_id_text, FILE *stream)
{
    unsigned char nonce[16];
    unsigned char new_nonce[32];
    unsigned char padding[96];
    unsigned char temp_key[32];
    unsigned char p_bytes[4];
    unsigned char q_bytes[4];
    unsigned char inner_data[160];
    unsigned char encrypted_data[TG_MTPROTO_RSA_PADDED_LENGTH];
    unsigned char client_encrypted[TG_MTPROTO_DH_ENCRYPTED_ANSWER_MAX];
    unsigned char auth_key[TG_MTPROTO_AUTH_KEY_LENGTH];
    unsigned char b[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char client_padding[15];
    unsigned char session_id[8];
    unsigned char ping_id_bytes[8];
    unsigned char encrypted_padding[64];
    unsigned char body[384];
    unsigned char payload[512];
    unsigned char packet[600];
    unsigned char response[1200];
    unsigned long body_length;
    unsigned long client_encrypted_length;
    unsigned long payload_length;
    unsigned long encrypted_padding_length;
    unsigned long response_length;
    unsigned long constructor;
    unsigned long ping_id_hi;
    unsigned long ping_id_lo;
    unsigned long p;
    unsigned long q;
    unsigned int i;
    long dc_id;
    tg_mtproto_message_id first_msg_id;
    tg_mtproto_message_id second_msg_id;
    tg_mtproto_message_id third_msg_id;
    tg_mtproto_res_pq res_pq;
    tg_mtproto_server_dh_params_ok params_ok;
    tg_mtproto_server_dh_inner_data inner;
    tg_mtproto_set_client_dh_answer dh_answer;
    tg_mtproto_encrypted_message decrypted;
    tg_mtproto_session session;
    tg_mtproto_tl_writer writer;
    tg_net_connection connection;
    tg_net_status net_status;
    const tg_mtproto_public_key *public_key;
    char error_buffer[160];

    if (host == 0 || port == 0 || stream == 0 ||
        tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0) {
        fputs("mtproto req_DH_params probe: invalid-arguments\n", stream);
        return 2;
    }

    tg_mtproto_probe_nonce(nonce);
    tg_mtproto_client_message_id((unsigned long)time(0), 4UL, 0,
                                 &first_msg_id);

    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_build_req_pq_multi(&writer, first_msg_id.hi,
                                      first_msg_id.lo, nonce) !=
        TG_MTPROTO_TL_OK) {
        fputs("mtproto req_DH_params probe: req_pq-build-failed\n", stream);
        return 2;
    }
    payload_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_init(&writer) != TG_MTPROTO_TL_OK ||
        tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
            TG_MTPROTO_TL_OK) {
        fputs("mtproto req_DH_params probe: req_pq-transport-build-failed\n",
              stream);
        return 2;
    }

    error_buffer[0] = '\0';
    net_status = tg_net_connect(&connection, host, port, error_buffer,
                                sizeof(error_buffer));
    if (net_status != TG_NET_OK) {
        fprintf(stream, "mtproto req_DH_params probe: connect-failed (%s)\n",
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
    if (net_status != TG_NET_OK) {
        tg_net_close(&connection);
        fprintf(stream, "mtproto req_DH_params probe: req_pq-failed (%s)\n",
                tg_net_status_name(net_status));
        return 2;
    }

    constructor = response_length >= 24UL ?
        tg_mtproto_read_u32_le(response + 20) : 0UL;
    if (constructor != 0x05162463UL ||
        tg_mtproto_parse_res_pq(response, response_length, &res_pq) !=
            TG_MTPROTO_TL_OK ||
        !tg_mtproto_res_pq_nonce_matches(&res_pq, nonce) ||
        tg_mtproto_pq_factor(res_pq.pq, res_pq.pq_length, &p, &q) != 0) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: resPQ-parse-failed\n", stream);
        return 2;
    }

    public_key = tg_mtproto_select_public_key(&res_pq);
    if (public_key == 0) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: rsa-key-not-found\n", stream);
        return 2;
    }

    tg_mtproto_u32_be(p, p_bytes);
    tg_mtproto_u32_be(q, q_bytes);
    tg_mtproto_probe_random(new_nonce, sizeof(new_nonce));
    tg_mtproto_probe_random(padding, sizeof(padding));
    tg_mtproto_probe_random(temp_key, sizeof(temp_key));

    tg_mtproto_tl_writer_init(&writer, inner_data, sizeof(inner_data));
    if (tg_mtproto_build_p_q_inner_data_dc(&writer, res_pq.pq,
                                           res_pq.pq_length, p_bytes,
                                           sizeof(p_bytes), q_bytes,
                                           sizeof(q_bytes), nonce,
                                           res_pq.server_nonce, new_nonce,
                                           dc_id) != TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: inner-build-failed\n", stream);
        return 2;
    }

    for (i = 0U; i < 32U; ++i) {
        if (tg_mtproto_rsa_pad(inner_data, writer.length, padding, temp_key,
                               public_key, encrypted_data) ==
            TG_MTPROTO_TL_OK) {
            break;
        }
        tg_mtproto_probe_random(temp_key, sizeof(temp_key));
    }
    if (i == 32U) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: rsa-pad-failed\n", stream);
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, body, sizeof(body));
    if (tg_mtproto_build_req_dh_params(&writer, nonce, res_pq.server_nonce,
                                       p_bytes, sizeof(p_bytes), q_bytes,
                                       sizeof(q_bytes),
                                       &public_key->fingerprint,
                                       encrypted_data) != TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: req-dh-build-failed\n", stream);
        return 2;
    }
    body_length = writer.length;
    tg_mtproto_client_message_id((unsigned long)time(0), 8UL, &first_msg_id,
                                 &second_msg_id);
    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_write_plain_message(&writer, second_msg_id.hi,
                                       second_msg_id.lo, body,
                                       body_length) != TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: envelope-build-failed\n", stream);
        return 2;
    }
    payload_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
        TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: transport-build-failed\n", stream);
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
    if (net_status != TG_NET_OK) {
        tg_net_close(&connection);
        fprintf(stream, "mtproto req_DH_params probe: req-dh-failed (%s)\n",
                tg_net_status_name(net_status));
        return 2;
    }

    constructor = response_length >= 24UL ?
        tg_mtproto_read_u32_le(response + 20) : 0UL;
    fprintf(stream,
            "mtproto req_DH_params probe: received %lu bytes, constructor 0x%08lx\n",
            response_length, constructor);
    if (constructor != 0xd0e8075cUL ||
        tg_mtproto_parse_server_dh_params_ok(response, response_length,
                                             &params_ok) != TG_MTPROTO_TL_OK ||
        memcmp(params_ok.nonce, nonce, 16U) != 0 ||
        memcmp(params_ok.server_nonce, res_pq.server_nonce, 16U) != 0 ||
        tg_mtproto_decrypt_server_dh_inner_data(
            params_ok.encrypted_answer, params_ok.encrypted_answer_length,
            new_nonce, nonce, res_pq.server_nonce, &inner) !=
            TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: server-dh-parse-failed\n", stream);
        return 2;
    }

    fprintf(stream,
            "mtproto req_DH_params probe: g %lu, dh_prime %lu bytes, g_a %lu bytes, server_time %lu\n",
            inner.g, inner.dh_prime_length, inner.g_a_length,
            inner.server_time);

    if (!tg_mtproto_check_dh_params(&inner)) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: dh-params-check-failed\n",
              stream);
        return 2;
    }
    tg_mtproto_probe_random(b, sizeof(b));
    tg_mtproto_probe_random(client_padding, sizeof(client_padding));
    if (tg_mtproto_build_client_dh_request(&inner, new_nonce, b,
                                           client_padding, client_encrypted,
                                           &client_encrypted_length,
                                           auth_key) != TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: client-dh-build-failed\n",
              stream);
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, body, sizeof(body));
    if (tg_mtproto_build_set_client_dh_params(
            &writer, nonce, res_pq.server_nonce, client_encrypted,
            client_encrypted_length) != TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: set-client-dh-build-failed\n",
              stream);
        return 2;
    }
    body_length = writer.length;
    tg_mtproto_client_message_id((unsigned long)time(0), 12UL,
                                 &second_msg_id, &third_msg_id);
    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_write_plain_message(&writer, third_msg_id.hi,
                                       third_msg_id.lo, body,
                                       body_length) != TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: set-client-envelope-failed\n",
              stream);
        return 2;
    }
    payload_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
        TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: set-client-transport-build-failed\n",
              stream);
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
    if (net_status != TG_NET_OK) {
        tg_net_close(&connection);
        fprintf(stream, "mtproto req_DH_params probe: set-client-dh-failed (%s)\n",
                tg_net_status_name(net_status));
        return 2;
    }

    constructor = response_length >= 24UL ?
        tg_mtproto_read_u32_le(response + 20) : 0UL;
    fprintf(stream,
            "mtproto req_DH_params probe: final received %lu bytes, constructor 0x%08lx\n",
            response_length, constructor);
    if (tg_mtproto_parse_set_client_dh_answer(response, response_length,
                                              &dh_answer) !=
        TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: set-client-dh-parse-failed\n",
              stream);
        return 2;
    }
    if (!tg_mtproto_verify_dh_gen_ok(&dh_answer, nonce, res_pq.server_nonce,
                                     new_nonce, auth_key)) {
        fprintf(stream,
                "mtproto req_DH_params probe: dh-gen-not-ok constructor 0x%08lx\n",
                dh_answer.constructor);
        tg_net_close(&connection);
        return 2;
    }
    fputs("mtproto req_DH_params probe: dh_gen_ok, auth_key derived in memory only\n",
          stream);

    tg_mtproto_probe_random(session_id, sizeof(session_id));
    tg_mtproto_session_from_auth_key(&session, (unsigned long)dc_id, auth_key,
                                     new_nonce, res_pq.server_nonce,
                                     session_id);
    tg_mtproto_probe_random(ping_id_bytes, sizeof(ping_id_bytes));
    ping_id_lo = tg_mtproto_read_u32_le(ping_id_bytes);
    ping_id_hi = tg_mtproto_read_u32_le(ping_id_bytes + 4U);
    tg_mtproto_tl_writer_init(&writer, body, sizeof(body));
    if (tg_mtproto_tl_write_u32(&writer, 0x7abe77ecUL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, ping_id_hi, ping_id_lo) !=
            TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: ping-build-failed\n", stream);
        return 2;
    }
    body_length = writer.length;
    encrypted_padding_length = 12UL;
    while (((32UL + body_length + encrypted_padding_length) % 16UL) != 0UL) {
        ++encrypted_padding_length;
    }
    tg_mtproto_probe_random(encrypted_padding, encrypted_padding_length);
    tg_mtproto_client_message_id((unsigned long)time(0), 16UL,
                                 &third_msg_id, &third_msg_id);
    tg_mtproto_tl_writer_init(&writer, payload, sizeof(payload));
    if (tg_mtproto_write_encrypted_message(
            &writer, auth_key, session.server_salt_hi,
            session.server_salt_lo, session.session_id, third_msg_id.hi,
            third_msg_id.lo, 1UL, body, body_length,
            encrypted_padding, encrypted_padding_length) !=
        TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: encrypted-ping-build-failed\n",
              stream);
        return 2;
    }
    payload_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, packet, sizeof(packet));
    if (tg_mtproto_write_abridged_packet(&writer, payload, payload_length) !=
        TG_MTPROTO_TL_OK) {
        tg_net_close(&connection);
        fputs("mtproto req_DH_params probe: encrypted-ping-transport-build-failed\n",
              stream);
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
        fprintf(stream, "mtproto req_DH_params probe: encrypted-ping-failed (%s)\n",
                tg_net_status_name(net_status));
        return 2;
    }
    if (tg_mtproto_decrypt_encrypted_message(response, response_length,
                                             auth_key, &decrypted) !=
        TG_MTPROTO_TL_OK) {
        fputs("mtproto req_DH_params probe: encrypted-response-decrypt-failed\n",
              stream);
        return 2;
    }
    constructor = decrypted.body_length >= 4UL ?
        tg_mtproto_read_u32_le(decrypted.body) : 0UL;
    fprintf(stream,
            "mtproto req_DH_params probe: encrypted response %lu bytes, constructor 0x%08lx\n",
            decrypted.body_length, constructor);
    if (tg_mtproto_body_is_expected_pong(decrypted.body,
                                         decrypted.body_length, ping_id_hi,
                                         ping_id_lo) ||
        tg_mtproto_container_has_expected_pong(decrypted.body,
                                               decrypted.body_length,
                                               ping_id_hi, ping_id_lo)) {
        fputs("mtproto req_DH_params probe: encrypted ping pong ok\n",
              stream);
        return 0;
    }
    if (constructor == 0xedab447bUL) {
        fputs("mtproto req_DH_params probe: bad_server_salt received\n",
              stream);
        return 2;
    }
    fputs("mtproto req_DH_params probe: encrypted-ping-unexpected-response\n",
          stream);
    return 2;
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
