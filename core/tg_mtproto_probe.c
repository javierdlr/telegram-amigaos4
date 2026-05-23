/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#if TG_ENABLE_GZIP || TG_ENABLE_GZIP_PUFF
#include <limits.h>
#endif

#if TG_ENABLE_GZIP
#include <zlib.h>
#endif

#if TG_ENABLE_GZIP_PUFF
#include "puff.h"
#endif

#include "tg_mtproto_auth.h"
#include "tg_mtproto_encrypted.h"
#include "tg_mtproto_envelope.h"
#include "tg_mtproto_login.h"
#include "tg_mtproto_message_id.h"
#include "tg_mtproto_probe.h"
#include "tg_mtproto_rsa.h"
#include "tg_mtproto_session.h"
#include "tg_mtproto_srp.h"
#include "tg_mtproto_transport.h"
#include "tg_console.h"
#include "tg_file.h"
#include "tg_net.h"
#include "tg_platform.h"

#define TG_MTPROTO_RPC_RESULT_CONSTRUCTOR 0xf35c6d01UL
#define TG_MTPROTO_RPC_ERROR_CONSTRUCTOR 0x2144ca19UL
#define TG_MTPROTO_MSG_CONTAINER_CONSTRUCTOR 0x73f1f8dcUL
#define TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR 0x3072cfa1UL
#define TG_MTPROTO_UPDATES_CONSTRUCTOR 0x74ae4240UL
#define TG_MTPROTO_UPDATES_COMBINED_CONSTRUCTOR 0x725b04c3UL
#define TG_MTPROTO_UPDATE_SHORT_CONSTRUCTOR 0x78d4dec1UL
#define TG_MTPROTO_UPDATE_SHORT_MESSAGE_CONSTRUCTOR 0x914fbf11UL
#define TG_MTPROTO_UPDATE_SHORT_CHAT_MESSAGE_CONSTRUCTOR 0x16812688UL
#define TG_MTPROTO_UPDATE_SHORT_SENT_MESSAGE_CONSTRUCTOR 0x9015e101UL
#define TG_MTPROTO_UPDATES_TOO_LONG_CONSTRUCTOR 0xe317af7eUL
#define TG_MTPROTO_AUTH_SENT_CODE_CONSTRUCTOR 0x5e002502UL
#define TG_MTPROTO_AUTH_SENT_CODE_SUCCESS_CONSTRUCTOR 0x2390fe44UL
#define TG_MTPROTO_AUTH_SENT_CODE_PAYMENT_REQUIRED_CONSTRUCTOR 0xd7a2fcf9UL
#define TG_MTPROTO_CONFIG_CONSTRUCTOR 0xcc1a241eUL
#define TG_MTPROTO_ACCOUNT_PASSWORD_CONSTRUCTOR 0x957b50fbUL
#define TG_MTPROTO_GZIP_UNPACKED_MAX 65536UL
#define TG_MTPROTO_PEER_USER_CONSTRUCTOR 0x59511722UL
#define TG_MTPROTO_PEER_CHAT_CONSTRUCTOR 0x36c6019aUL
#define TG_MTPROTO_PEER_CHANNEL_CONSTRUCTOR 0xa2a5371eUL
#define TG_MTPROTO_PHONE_MIGRATE_RC_BASE 40

typedef struct tg_mtproto_auth_context {
    tg_net_connection connection;
    tg_mtproto_session session;
    tg_mtproto_message_id last_msg_id;
    unsigned char auth_key[TG_MTPROTO_AUTH_KEY_LENGTH];
    long server_time_delta_seconds;
    int connection_open;
} tg_mtproto_auth_context;

#if TG_ENABLE_GZIP || TG_ENABLE_GZIP_PUFF
static unsigned char tg_mtproto_gzip_unpacked[TG_MTPROTO_GZIP_UNPACKED_MAX];
#endif

static int tg_mtproto_auth_check_password_text(const char *host,
                                               const char *port,
                                               const char *api_id_text,
                                               const char *auth_file,
                                               const char *dc_id_text,
                                               const char *password_input,
                                               FILE *stream);

static int tg_mtproto_production_endpoint_for_dc(unsigned long dc_id,
                                                 const char **host,
                                                 const char **dc_id_text)
{
    if (host == 0 || dc_id_text == 0) {
        return 1;
    }
    switch (dc_id) {
    case 1UL:
        *host = "149.154.175.50";
        *dc_id_text = "1";
        return 0;
    case 2UL:
        *host = "149.154.167.50";
        *dc_id_text = "2";
        return 0;
    case 3UL:
        *host = "149.154.175.100";
        *dc_id_text = "3";
        return 0;
    case 4UL:
        *host = "149.154.167.91";
        *dc_id_text = "4";
        return 0;
    case 5UL:
        *host = "91.108.56.130";
        *dc_id_text = "5";
        return 0;
    default:
        return 1;
    }
}

static int tg_mtproto_parse_phone_migrate_dc(const char *message,
                                             unsigned long *dc_id)
{
    unsigned long value;
    const char *digits;

    if (message == 0 || dc_id == 0) {
        return 0;
    }
    if (strncmp(message, "PHONE_MIGRATE_", 14) != 0) {
        return 0;
    }
    digits = message + 14;
    if (*digits < '1' || *digits > '9') {
        return 0;
    }
    value = 0UL;
    while (*digits >= '0' && *digits <= '9') {
        value = (value * 10UL) + (unsigned long)(*digits - '0');
        ++digits;
    }
    if (*digits != '\0' || value == 0UL || value > 255UL) {
        return 0;
    }
    *dc_id = value;
    return 1;
}

static int tg_mtproto_is_async_update_constructor(unsigned long constructor)
{
    return constructor == TG_MTPROTO_UPDATES_CONSTRUCTOR ||
           constructor == TG_MTPROTO_UPDATES_COMBINED_CONSTRUCTOR ||
           constructor == TG_MTPROTO_UPDATE_SHORT_CONSTRUCTOR ||
           constructor == TG_MTPROTO_UPDATE_SHORT_MESSAGE_CONSTRUCTOR ||
           constructor == TG_MTPROTO_UPDATE_SHORT_CHAT_MESSAGE_CONSTRUCTOR ||
           constructor == TG_MTPROTO_UPDATE_SHORT_SENT_MESSAGE_CONSTRUCTOR ||
           constructor == TG_MTPROTO_UPDATES_TOO_LONG_CONSTRUCTOR;
}

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

static void tg_mtproto_saved_session_random(unsigned char *bytes,
                                            unsigned long length)
{
    if (!tg_platform_random_bytes(bytes, length)) {
        tg_mtproto_probe_random(bytes, length);
    }
}

static unsigned long tg_mtproto_context_time(
    const tg_mtproto_auth_context *context)
{
    unsigned long now;
    unsigned long delta;

    now = (unsigned long)time(0);
    if (context == 0 || context->server_time_delta_seconds == 0L) {
        return now;
    }
    if (context->server_time_delta_seconds < 0L) {
        delta = (unsigned long)(0L - context->server_time_delta_seconds);
        return now > delta ? now - delta : 0UL;
    }
    return now + (unsigned long)context->server_time_delta_seconds;
}

static void tg_mtproto_sync_time_from_server(
    tg_mtproto_auth_context *context,
    const tg_mtproto_encrypted_message *message)
{
    unsigned long now;
    unsigned long server_time;
    unsigned long delta;

    if (context == 0 || message == 0 || message->message_id_hi == 0UL) {
        return;
    }

    now = (unsigned long)time(0);
    server_time = message->message_id_hi;
    if (server_time >= now) {
        delta = server_time - now;
        context->server_time_delta_seconds = (long)delta;
    } else {
        delta = now - server_time;
        context->server_time_delta_seconds = -((long)delta);
    }

    context->last_msg_id.hi = message->message_id_hi;
    context->last_msg_id.lo = message->message_id_lo;
    context->session.last_msg_id_hi = context->last_msg_id.hi;
    context->session.last_msg_id_lo = context->last_msg_id.lo;
}

static void tg_mtproto_refresh_saved_session(
    tg_mtproto_auth_context *context)
{
    if (context == 0) {
        return;
    }
    tg_mtproto_saved_session_random(context->session.session_id,
                                    sizeof(context->session.session_id));
    context->session.seq_no = 1UL;
    tg_mtproto_client_message_id(tg_mtproto_context_time(context), 4UL, 0,
                                 &context->last_msg_id);
    context->session.last_msg_id_hi = context->last_msg_id.hi;
    context->session.last_msg_id_lo = context->last_msg_id.lo;
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
    const char *number_text;
    int is_test_dc;

    if (text == 0 || text[0] == '\0' || dc_id == 0) {
        return 1;
    }
    is_test_dc = 0;
    number_text = text;
    if (strncmp(text, "test:", 5U) == 0) {
        is_test_dc = 1;
        number_text = text + 5U;
    }
    value = strtol(number_text, &endptr, 10);
    if (endptr == number_text || *endptr != '\0' || value < -100000L ||
        value > 100000L) {
        return 1;
    }
    if (is_test_dc) {
        if (value < 1L || value > 9999L) {
            return 1;
        }
        value += 10000L;
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

static int tg_mtproto_validate_saved_auth_dc(
    const tg_mtproto_auth_context *context,
    unsigned long requested_dc,
    FILE *stream,
    const char *label)
{
    if (context == 0 || stream == 0 || label == 0 || requested_dc == 0UL) {
        return 2;
    }
    if (context->session.dc_id != 0UL &&
        context->session.dc_id != requested_dc) {
        fprintf(stream,
                "%s: auth-dc-mismatch auth-file-dc %lu requested-dc %lu\n",
                label, context->session.dc_id, requested_dc);
        return 2;
    }
    return 0;
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
    tg_mtproto_saved_session_random(encrypted_padding,
                                    encrypted_padding_length);
    tg_mtproto_client_message_id(tg_mtproto_context_time(context), 20UL,
                                 &context->last_msg_id, &msg_id);
    context->last_msg_id = msg_id;
    context->session.last_msg_id_hi = msg_id.hi;
    context->session.last_msg_id_lo = msg_id.lo;
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
    int retry_request;
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

    for (attempt = 0U; attempt < 6U; ++attempt) {
        retry_request = 0;
        encrypted_padding_length = 12UL;
        while (((32UL + body_length + encrypted_padding_length) % 16UL) !=
               0UL) {
            ++encrypted_padding_length;
        }
        tg_mtproto_saved_session_random(encrypted_padding,
                                        encrypted_padding_length);
        tg_mtproto_client_message_id(tg_mtproto_context_time(context), 16UL,
                                     &context->last_msg_id, &request_msg_id);
        context->last_msg_id = request_msg_id;
        context->session.last_msg_id_hi = request_msg_id.hi;
        context->session.last_msg_id_lo = request_msg_id.lo;

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
        for (receive_attempt = 0U; receive_attempt < 32U; ++receive_attempt) {
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
                    retry_request = 1;
                    break;
                }
                if (bad_msg.error_code == 32UL) {
                    context->session.seq_no += 2UL;
                    retry_request = 1;
                    break;
                }
                if (bad_msg.error_code == 33UL &&
                    context->session.seq_no >= 2UL) {
                    context->session.seq_no -= 2UL;
                    retry_request = 1;
                    break;
                }
                if (bad_msg.error_code == 16UL ||
                    bad_msg.error_code == 17UL) {
                    tg_mtproto_sync_time_from_server(context, &decrypted);
                    retry_request = 1;
                    break;
                }
                fprintf(stream, "%s: bad-msg error-code %lu\n", label,
                        bad_msg.error_code);
                return 2;
            }
            response_constructor = decrypted.body_length >= 4UL ?
                tg_mtproto_read_u32_le(decrypted.body) : 0UL;
            if (response_constructor == TG_MTPROTO_RPC_RESULT_CONSTRUCTOR) {
                continue;
            }
            if (tg_mtproto_is_async_update_constructor(response_constructor)) {
                continue;
            }
            if (response_constructor != TG_MTPROTO_MSG_CONTAINER_CONSTRUCTOR &&
                response_constructor != 0x9ec20908UL) {
                fprintf(stream,
                        "%s: encrypted-response-unexpected constructor 0x%08lx\n",
                        label, response_constructor);
                return 2;
            }
        }
        if (retry_request) {
            continue;
        }
        fprintf(stream, "%s: rpc-response-not-received\n", label);
        return 2;
    }

    fprintf(stream, "%s: bad-msg-retry-failed\n", label);
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
        fprintf(stream,
                "%s: server-dh-parse-failed response-bytes %lu first-word 0x%08lx constructor 0x%08lx\n",
                label, response_length,
                response_length >= 4UL ? tg_mtproto_read_u32_le(response) : 0UL,
                constructor);
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
    context->session.last_msg_id_hi = third_msg_id.hi;
    context->session.last_msg_id_lo = third_msg_id.lo;
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

    tg_mtproto_refresh_saved_session(context);
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

static void tg_mtproto_trim_newline(char *text)
{
    unsigned long length;

    if (text == 0) {
        return;
    }
    length = (unsigned long)strlen(text);
    while (length > 0UL &&
           (text[length - 1U] == '\n' || text[length - 1U] == '\r')) {
        text[length - 1U] = '\0';
        --length;
    }
}

static void tg_mtproto_secure_zero(void *data, unsigned long length)
{
    volatile unsigned char *bytes;

    bytes = (volatile unsigned char *)data;
    while (length > 0UL) {
        *bytes = 0U;
        ++bytes;
        --length;
    }
}

static int tg_mtproto_load_password_file(const char *path,
                                         char *password,
                                         unsigned long password_size,
                                         unsigned long *password_length,
                                         FILE *stream,
                                         const char *label)
{
    tg_file_status file_status;

    if (password_length != 0) {
        *password_length = 0UL;
    }
    if (path == 0 || password == 0 || password_size == 0UL ||
        password_length == 0) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: password-file-invalid\n", label);
        }
        return 2;
    }
    file_status = tg_file_read_text(path, password, password_size,
                                    password_length);
    if (file_status == TG_FILE_TOO_LARGE) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: password-file-too-large\n", label);
        }
        return 2;
    }
    if (file_status != TG_FILE_OK) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: password-file-load-failed (%s)\n", label,
                    tg_file_status_name(file_status));
        }
        return 2;
    }
    tg_mtproto_trim_newline(password);
    *password_length = (unsigned long)strlen(password);
    if (*password_length == 0UL) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: password-file-empty\n", label);
        }
        tg_mtproto_secure_zero(password, password_size);
        return 2;
    }
    return 0;
}

static int tg_mtproto_prompt_line(const char *prompt,
                                  char *out,
                                  unsigned long out_size,
                                  int required,
                                  FILE *stream,
                                  const char *label)
{
    if (out != 0 && out_size > 0UL) {
        out[0] = '\0';
    }
    if (prompt == 0 || out == 0 || out_size == 0UL || stream == 0 ||
        label == 0) {
        return 2;
    }
    fputs(prompt, stream);
    fflush(stream);
    if (fgets(out, (int)out_size, stdin) == 0) {
        fprintf(stream, "%s: input-closed\n", label);
        return 2;
    }
    tg_mtproto_trim_line(out);
    if (required && out[0] == '\0') {
        fprintf(stream, "%s: input-empty\n", label);
        return 2;
    }
    return 0;
}

static void tg_mtproto_copy_trimmed_field(const char *source,
                                          unsigned long source_length,
                                          char *out,
                                          unsigned long out_size)
{
    unsigned long start;
    unsigned long end;
    unsigned long length;

    if (out == 0 || out_size == 0UL) {
        return;
    }
    out[0] = '\0';
    if (source == 0) {
        return;
    }
    start = 0UL;
    while (start < source_length &&
           (source[start] == ' ' || source[start] == '\t')) {
        ++start;
    }
    end = source_length;
    while (end > start &&
           (source[end - 1UL] == ' ' || source[end - 1UL] == '\t' ||
            source[end - 1UL] == '\r' || source[end - 1UL] == '\n')) {
        --end;
    }
    length = end - start;
    if (length >= out_size) {
        length = out_size - 1UL;
    }
    if (length > 0UL) {
        memcpy(out, source + start, (size_t)length);
    }
    out[length] = '\0';
}

static int tg_mtproto_load_api_credentials(const char *path,
                                           char *api_id,
                                           unsigned long api_id_size,
                                           char *api_hash,
                                           unsigned long api_hash_size,
                                           FILE *stream,
                                           const char *label)
{
    char text[256];
    unsigned long text_length;
    unsigned long offset;
    unsigned long line_start;
    unsigned int field;
    tg_file_status file_status;

    if (api_id != 0 && api_id_size > 0UL) {
        api_id[0] = '\0';
    }
    if (api_hash != 0 && api_hash_size > 0UL) {
        api_hash[0] = '\0';
    }
    if (path == 0 || api_id == 0 || api_hash == 0 ||
        api_id_size == 0UL || api_hash_size == 0UL) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: api-file-invalid\n", label);
        }
        return 2;
    }

    file_status = tg_file_read_text(path, text, sizeof(text), &text_length);
    if (file_status == TG_FILE_TOO_LARGE) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: api-file-too-large\n", label);
        }
        return 2;
    }
    if (file_status != TG_FILE_OK) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: api-file-load-failed (%s)\n", label,
                    tg_file_status_name(file_status));
        }
        return 2;
    }

    field = 0U;
    offset = 0UL;
    while (offset <= text_length && field < 2U) {
        line_start = offset;
        while (offset < text_length && text[offset] != '\n' &&
               text[offset] != '\r') {
            ++offset;
        }
        if (offset > line_start) {
            if (field == 0U) {
                tg_mtproto_copy_trimmed_field(text + line_start,
                                              offset - line_start,
                                              api_id, api_id_size);
                if (api_id[0] != '\0') {
                    ++field;
                }
            } else {
                tg_mtproto_copy_trimmed_field(text + line_start,
                                              offset - line_start,
                                              api_hash, api_hash_size);
                if (api_hash[0] != '\0') {
                    ++field;
                }
            }
        }
        while (offset < text_length &&
               (text[offset] == '\n' || text[offset] == '\r')) {
            ++offset;
        }
        if (offset == text_length) {
            break;
        }
    }
    tg_mtproto_secure_zero(text, sizeof(text));
    if (api_id[0] == '\0' || api_hash[0] == '\0') {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: api-file-incomplete\n", label);
        }
        tg_mtproto_secure_zero(api_hash, api_hash_size);
        return 2;
    }
    return 0;
}

static int tg_mtproto_load_api_id_file(const char *path,
                                       char *api_id,
                                       unsigned long api_id_size,
                                       FILE *stream,
                                       const char *label)
{
    char text[256];
    unsigned long text_length;
    unsigned long offset;
    unsigned long line_start;
    tg_file_status file_status;

    if (api_id != 0 && api_id_size > 0UL) {
        api_id[0] = '\0';
    }
    if (path == 0 || api_id == 0 || api_id_size == 0UL) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: api-file-invalid\n", label);
        }
        return 2;
    }

    file_status = tg_file_read_text(path, text, sizeof(text), &text_length);
    if (file_status == TG_FILE_TOO_LARGE) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: api-file-too-large\n", label);
        }
        return 2;
    }
    if (file_status != TG_FILE_OK) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: api-file-load-failed (%s)\n", label,
                    tg_file_status_name(file_status));
        }
        return 2;
    }

    offset = 0UL;
    while (offset <= text_length) {
        line_start = offset;
        while (offset < text_length && text[offset] != '\n' &&
               text[offset] != '\r') {
            ++offset;
        }
        if (offset > line_start) {
            tg_mtproto_copy_trimmed_field(text + line_start,
                                          offset - line_start,
                                          api_id, api_id_size);
            if (api_id[0] != '\0') {
                break;
            }
        }
        while (offset < text_length &&
               (text[offset] == '\n' || text[offset] == '\r')) {
            ++offset;
        }
        if (offset == text_length) {
            break;
        }
    }
    tg_mtproto_secure_zero(text, sizeof(text));
    if (api_id[0] == '\0') {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: api-file-incomplete\n", label);
        }
        return 2;
    }
    return 0;
}

static int tg_mtproto_check_secret_file_permissions(const char *label,
                                                    const char *path,
                                                    FILE *stream)
{
#if defined(S_IRWXG) && defined(S_IRWXO)
    struct stat status;

    if (label == 0 || path == 0 || path[0] == '\0' || stream == 0) {
        return 0;
    }
    if (stat(path, &status) != 0) {
        return 0;
    }
    if ((status.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        fprintf(stream,
                "mtproto local-files: warning %s permissions are broad\n",
                label);
        return 1;
    }
#else
    (void)label;
    (void)path;
    (void)stream;
#endif
    return 0;
}

static int tg_mtproto_check_code_hash_file(const char *path,
                                           FILE *stream,
                                           const char *label)
{
    char text[256];
    unsigned long text_length;
    tg_file_status file_status;

    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    file_status = tg_file_read_text(path, text, sizeof(text), &text_length);
    if (file_status == TG_FILE_TOO_LARGE) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: code-hash-file-too-large\n", label);
        }
        return 2;
    }
    if (file_status != TG_FILE_OK) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: code-hash-file-load-failed (%s)\n", label,
                    tg_file_status_name(file_status));
        }
        return 2;
    }
    tg_mtproto_trim_newline(text);
    if (text[0] == '\0') {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: code-hash-file-empty\n", label);
        }
        return 2;
    }
    return 0;
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
    } else if (strcmp(error_message, "PASSWORD_HASH_INVALID") == 0) {
        fprintf(stream, "%s: password-invalid\n", label);
    } else if (strcmp(error_message, "SRP_ID_INVALID") == 0) {
        fprintf(stream, "%s: srp-id-invalid\n", label);
    } else if (strcmp(error_message, "PASSWORD_MISSING") == 0) {
        fprintf(stream, "%s: password-missing\n", label);
    } else if (strcmp(error_message, "AUTH_KEY_UNREGISTERED") == 0) {
        fprintf(stream, "%s: auth-key-unregistered\n", label);
    } else {
        fprintf(stream, "%s: rpc-error %ld %s\n", label, error_code,
                error_message);
    }
    return 1;
}

static int tg_mtproto_rpc_phone_migrate_dc(
    const tg_mtproto_rpc_result *result,
    unsigned long *dc_id)
{
    char error_message[128];
    long error_code;

    if (result == 0 || dc_id == 0 ||
        result->result_constructor != TG_MTPROTO_RPC_ERROR_CONSTRUCTOR ||
        tg_mtproto_parse_rpc_error(result->result_body - 4U,
                                   result->result_body_length + 4U,
                                   &error_code, error_message,
                                   sizeof(error_message)) !=
            TG_MTPROTO_TL_OK) {
        return 0;
    }
    (void)error_code;
    return tg_mtproto_parse_phone_migrate_dc(error_message, dc_id);
}

#if TG_ENABLE_GZIP_PUFF
static int tg_mtproto_gzip_skip_zero_string(const unsigned char *data,
                                            unsigned long length,
                                            unsigned long *offset)
{
    if (data == 0 || offset == 0 || *offset >= length) {
        return 2;
    }
    while (*offset < length && data[*offset] != 0U) {
        ++(*offset);
    }
    if (*offset >= length) {
        return 2;
    }
    ++(*offset);
    return 0;
}

static int tg_mtproto_gzip_unpack_puff(const unsigned char *packed_data,
                                       unsigned long packed_length,
                                       unsigned long *unpacked_length)
{
    unsigned long offset;
    unsigned long extra_length;
    unsigned long source_length;
    unsigned long dest_length;
    unsigned int flags;
    int rc;

    if (unpacked_length != 0) {
        *unpacked_length = 0UL;
    }
    if (packed_data == 0 || unpacked_length == 0 || packed_length < 18UL ||
        packed_data[0] != 0x1fU || packed_data[1] != 0x8bU ||
        packed_data[2] != 8U) {
        return 2;
    }

    flags = (unsigned int)packed_data[3];
    if ((flags & 0xe0U) != 0U) {
        return 2;
    }
    offset = 10UL;

    if ((flags & 4U) != 0U) {
        if (packed_length - offset < 2UL) {
            return 2;
        }
        extra_length = ((unsigned long)packed_data[offset]) |
                       (((unsigned long)packed_data[offset + 1U]) << 8);
        offset += 2UL;
        if (extra_length > packed_length - offset) {
            return 2;
        }
        offset += extra_length;
    }
    if ((flags & 8U) != 0U &&
        tg_mtproto_gzip_skip_zero_string(packed_data, packed_length,
                                         &offset) != 0) {
        return 2;
    }
    if ((flags & 16U) != 0U &&
        tg_mtproto_gzip_skip_zero_string(packed_data, packed_length,
                                         &offset) != 0) {
        return 2;
    }
    if ((flags & 2U) != 0U) {
        if (packed_length - offset < 2UL) {
            return 2;
        }
        offset += 2UL;
    }
    if (packed_length - offset < 8UL) {
        return 2;
    }

    source_length = packed_length - offset - 8UL;
    dest_length = TG_MTPROTO_GZIP_UNPACKED_MAX;
    rc = puff(tg_mtproto_gzip_unpacked, &dest_length,
              packed_data + offset, &source_length);
    if (rc != 0 || dest_length < 4UL) {
        return 2;
    }
    *unpacked_length = dest_length;
    return 0;
}
#endif

static int tg_mtproto_unpack_gzip_result(tg_mtproto_rpc_result *result,
                                         FILE *stream,
                                         const char *label)
{
    const unsigned char *packed_data;
    unsigned long packed_length;
    tg_mtproto_tl_reader reader;

    if (result == 0 || stream == 0 || label == 0 ||
        result->result_constructor != TG_MTPROTO_GZIP_PACKED_CONSTRUCTOR) {
        return 0;
    }

    tg_mtproto_tl_reader_init(&reader, result->result_body,
                              result->result_body_length);
    if (tg_mtproto_tl_read_bytes(&reader, &packed_data, &packed_length) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: gzip-packed-parse-failed\n", label);
        return 2;
    }

#if TG_ENABLE_GZIP
    {
        int zrc;
        z_stream zs;

        if (packed_length > (unsigned long)UINT_MAX ||
            TG_MTPROTO_GZIP_UNPACKED_MAX > (unsigned long)UINT_MAX) {
            fprintf(stream, "%s: gzip-packed-too-large\n", label);
            return 2;
        }

        memset(&zs, 0, sizeof(zs));
        zs.next_in = (Bytef *)packed_data;
        zs.avail_in = (uInt)packed_length;
        zs.next_out = (Bytef *)tg_mtproto_gzip_unpacked;
        zs.avail_out = (uInt)TG_MTPROTO_GZIP_UNPACKED_MAX;

        zrc = inflateInit2(&zs, 16 + MAX_WBITS);
        if (zrc == Z_OK) {
            zrc = inflate(&zs, Z_FINISH);
            (void)inflateEnd(&zs);
        }
        if (zrc != Z_STREAM_END || zs.total_out < 4UL) {
            fprintf(stream, "%s: gzip-unpack-failed\n", label);
            return 2;
        }

        result->result_constructor =
            tg_mtproto_read_u32_le(tg_mtproto_gzip_unpacked);
        result->result_body = tg_mtproto_gzip_unpacked + 4;
        result->result_body_length = (unsigned long)zs.total_out - 4UL;
        return 0;
    }
#elif TG_ENABLE_GZIP_PUFF
    {
        unsigned long unpacked_length;

        if (tg_mtproto_gzip_unpack_puff(packed_data, packed_length,
                                        &unpacked_length) != 0) {
            fprintf(stream, "%s: gzip-unpack-failed\n", label);
            return 2;
        }

        result->result_constructor =
            tg_mtproto_read_u32_le(tg_mtproto_gzip_unpacked);
        result->result_body = tg_mtproto_gzip_unpacked + 4;
        result->result_body_length = unpacked_length - 4UL;
        return 0;
    }
#else
    (void)packed_data;
    (void)packed_length;
    fprintf(stream, "%s: gzip-packed-response-unsupported\n", label);
    return 2;
#endif
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

static int tg_mtproto_send_saved_query(const char *host,
                                       const char *port,
                                       const char *api_id_text,
                                       const char *auth_file,
                                       const char *dc_id_text,
                                       const unsigned char *query,
                                       unsigned long query_length,
                                       tg_mtproto_rpc_result *result,
                                       FILE *stream,
                                       const char *label)
{
    unsigned char wrapped_query[760];
    unsigned long api_id;
    tg_mtproto_auth_context context;
    tg_mtproto_session_status session_status;
    tg_mtproto_tl_writer writer;
    long dc_id;

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 || query == 0 || query_length == 0UL ||
        result == 0 || label == 0 ||
        tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0 ||
        tg_mtproto_parse_ulong_arg(api_id_text, &api_id) != 0) {
        if (stream != 0 && label != 0) {
            fprintf(stream, "%s: invalid-arguments\n", label);
        }
        return 2;
    }
    if (tg_mtproto_load_auth_context(host, port, auth_file, &context, stream,
                                     label) != 0) {
        return 2;
    }
    if (tg_mtproto_validate_saved_auth_dc(&context, (unsigned long)dc_id,
                                          stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        return 2;
    }

    if (tg_mtproto_build_initialized_query(&writer, wrapped_query,
                                           sizeof(wrapped_query), api_id,
                                           query, query_length) != 0) {
        tg_mtproto_close_auth_context(&context);
        fprintf(stream, "%s: init-connection-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_encrypted_query(&context, wrapped_query, writer.length,
                                        result, stream, label) != 0) {
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
    return 0;
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
        unsigned long migrate_dc;
        if (tg_mtproto_rpc_phone_migrate_dc(&result, &migrate_dc)) {
            if (migrate_dc <= 215UL) {
                return TG_MTPROTO_PHONE_MIGRATE_RC_BASE + (int)migrate_dc;
            }
        }
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
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

    fprintf(stream, "Login code sent.\n");
    return 0;
}

int tg_mtproto_auth_send_code_file(const char *host,
                                   const char *port,
                                   const char *dc_id_text,
                                   const char *api_file,
                                   const char *phone_number,
                                   const char *auth_file,
                                   const char *code_hash_file,
                                   FILE *stream)
{
    char api_id[32];
    char api_hash[96];
    int rc;
    static const char label[] = "mtproto auth.sendCode";

    if (tg_mtproto_load_api_credentials(api_file, api_id, sizeof(api_id),
                                        api_hash, sizeof(api_hash),
                                        stream, label) != 0) {
        return 2;
    }
    rc = tg_mtproto_auth_send_code(host, port, dc_id_text, api_id, api_hash,
                                   phone_number, auth_file, code_hash_file,
                                   stream);
    tg_mtproto_secure_zero(api_hash, sizeof(api_hash));
    return rc;
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
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
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

int tg_mtproto_auth_sign_in_file(const char *host,
                                 const char *port,
                                 const char *api_file,
                                 const char *auth_file,
                                 const char *phone_number,
                                 const char *code_hash_file,
                                 const char *phone_code,
                                 const char *dc_id_text,
                                 FILE *stream)
{
    char api_id[32];
    int rc;
    static const char label[] = "mtproto auth.signIn";

    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    rc = tg_mtproto_auth_sign_in(host, port, api_id, auth_file, phone_number,
                                 code_hash_file, phone_code, dc_id_text,
                                 stream);
    return rc;
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
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
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
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
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

int tg_mtproto_auth_get_config_file(const char *host,
                                    const char *port,
                                    const char *api_file,
                                    const char *auth_file,
                                    const char *dc_id_text,
                                    FILE *stream)
{
    char api_id[32];
    int rc;
    static const char label[] = "mtproto help.getConfig";

    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    rc = tg_mtproto_auth_get_config(host, port, api_id, auth_file,
                                    dc_id_text, stream);
    return rc;
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
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
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
    if (password.has_current_algo) {
        fprintf(stream, "%s: current_algo 0x%08lx\n", label,
                password.current_algo_constructor);
    }
    if (password.current_algo_constructor == 0x3a912d4aUL) {
        fprintf(stream,
                "%s: srp params salt1 %lu salt2 %lu p %lu B %lu g %lu srp_id 0x%08lx%08lx\n",
                label, password.current_salt1_length,
                password.current_salt2_length, password.current_p_length,
                password.srp_b_length, password.current_g,
                password.srp_id_hi, password.srp_id_lo);
    }
    fprintf(stream, "%s: srp_check pending\n", label);
    return 0;
}

int tg_mtproto_auth_get_password_file(const char *host,
                                      const char *port,
                                      const char *api_file,
                                      const char *auth_file,
                                      const char *dc_id_text,
                                      FILE *stream)
{
    char api_id[32];
    int rc;
    static const char label[] = "mtproto account.getPassword";

    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    rc = tg_mtproto_auth_get_password(host, port, api_id, auth_file,
                                      dc_id_text, stream);
    return rc;
}

static int tg_mtproto_auth_check_password_text(const char *host,
                                               const char *port,
                                               const char *api_id_text,
                                               const char *auth_file,
                                               const char *dc_id_text,
                                               const char *password_input,
                                               FILE *stream)
{
    unsigned char query[512];
    unsigned char wrapped_query[760];
    unsigned char random_a[TG_MTPROTO_SRP_VALUE_LENGTH];
    char password_text[512];
    unsigned long password_length;
    unsigned long api_id;
    tg_mtproto_auth_context context;
    tg_mtproto_password_summary password;
    tg_mtproto_rpc_result result;
    tg_mtproto_session_status session_status;
    tg_mtproto_srp_proof proof;
    tg_mtproto_tl_writer writer;
    long dc_id;
    static const char label[] = "mtproto auth.checkPassword";

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 || password_input == 0 ||
        tg_mtproto_parse_dc_id(dc_id_text, &dc_id) != 0 ||
        tg_mtproto_parse_ulong_arg(api_id_text, &api_id) != 0) {
        if (stream != 0) {
            fputs("mtproto auth.checkPassword: invalid-arguments\n", stream);
        }
        return 2;
    }

    password_length = (unsigned long)strlen(password_input);
    if (password_length == 0UL || password_length >= sizeof(password_text)) {
        fprintf(stream, "%s: password-invalid\n", label);
        return 2;
    }
    memcpy(password_text, password_input, password_length + 1UL);

    if (tg_mtproto_load_auth_context(host, port, auth_file, &context, stream,
                                     label) != 0) {
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        return 2;
    }
    context.session.dc_id = (unsigned long)dc_id;

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_account_get_password(&writer) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_build_initialized_query(&writer, wrapped_query,
                                           sizeof(wrapped_query), api_id,
                                           query, writer.length) != 0) {
        tg_mtproto_close_auth_context(&context);
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        fprintf(stream, "%s: get-password-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_encrypted_query(&context, wrapped_query, writer.length,
                                        &result, stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        tg_mtproto_close_auth_context(&context);
        session_status = tg_mtproto_session_save_authorization(
            auth_file, &context.session, context.auth_key, 1);
        if (session_status != TG_MTPROTO_SESSION_OK) {
            fprintf(stream, "%s: auth-file-save-failed (%s)\n", label,
                    tg_mtproto_session_status_name(session_status));
        } else if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        tg_mtproto_close_auth_context(&context);
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        return 2;
    }
    if (tg_mtproto_parse_account_password_summary(result.result_constructor,
                                                  result.result_body,
                                                  result.result_body_length,
                                                  &password) !=
        TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        fprintf(stream, "%s: password-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    if (!password.has_password || !password.has_current_algo) {
        tg_mtproto_close_auth_context(&context);
        session_status = tg_mtproto_session_save_authorization(
            auth_file, &context.session, context.auth_key, 1);
        if (session_status != TG_MTPROTO_SESSION_OK) {
            fprintf(stream, "%s: auth-file-save-failed (%s)\n", label,
                    tg_mtproto_session_status_name(session_status));
            tg_mtproto_secure_zero(password_text, sizeof(password_text));
            return 2;
        }
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        fprintf(stream, "%s: no password required\n", label);
        fprintf(stream, "%s: auth state updated\n", label);
        return 0;
    }
    if (!tg_mtproto_secure_random(random_a, sizeof(random_a))) {
        tg_mtproto_close_auth_context(&context);
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        fprintf(stream, "%s: secure-rng-unavailable\n", label);
        return 2;
    }
    if (tg_mtproto_srp_make_proof(&password,
                                  (const unsigned char *)password_text,
                                  password_length, random_a, &proof) !=
        TG_MTPROTO_TL_OK) {
        tg_mtproto_close_auth_context(&context);
        tg_mtproto_secure_zero(random_a, sizeof(random_a));
        tg_mtproto_secure_zero(password_text, sizeof(password_text));
        fprintf(stream, "%s: srp-proof-build-failed\n", label);
        return 2;
    }
    tg_mtproto_secure_zero(random_a, sizeof(random_a));
    tg_mtproto_secure_zero(password_text, sizeof(password_text));

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_check_password_srp(
            &writer, password.srp_id_hi, password.srp_id_lo,
            proof.a, proof.a_length, proof.m1) != TG_MTPROTO_TL_OK ||
        tg_mtproto_build_initialized_query(&writer, wrapped_query,
                                           sizeof(wrapped_query), api_id,
                                           query, writer.length) != 0) {
        tg_mtproto_close_auth_context(&context);
        tg_mtproto_secure_zero(&proof, sizeof(proof));
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    tg_mtproto_secure_zero(&proof, sizeof(proof));
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
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        return 2;
    }
    if (!tg_mtproto_is_auth_authorization_constructor(
            result.result_constructor)) {
        fprintf(stream, "%s: unexpected-result 0x%08lx\n", label,
                result.result_constructor);
        return 2;
    }
    if (result.result_constructor == 0x44747e9aUL) {
        fprintf(stream, "%s: signup-required\n", label);
        return 2;
    }

    fprintf(stream, "%s: signed in\n", label);
    fprintf(stream, "%s: auth state updated\n", label);
    return 0;
}

int tg_mtproto_auth_check_password(const char *host,
                                   const char *port,
                                   const char *api_id_text,
                                   const char *auth_file,
                                   const char *dc_id_text,
                                   const char *password_file,
                                   FILE *stream)
{
    char password_text[512];
    unsigned long password_length;
    int rc;
    static const char label[] = "mtproto auth.checkPassword";

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 || password_file == 0) {
        if (stream != 0) {
            fputs("mtproto auth.checkPassword: invalid-arguments\n", stream);
        }
        return 2;
    }

    if (tg_mtproto_load_password_file(password_file, password_text,
                                      sizeof(password_text),
                                      &password_length, stream,
                                      label) != 0) {
        return 2;
    }
    (void)password_length;
    rc = tg_mtproto_auth_check_password_text(host, port, api_id_text,
                                             auth_file, dc_id_text,
                                             password_text, stream);
    tg_mtproto_secure_zero(password_text, sizeof(password_text));
    return rc;
}

int tg_mtproto_auth_check_password_file(const char *host,
                                        const char *port,
                                        const char *api_file,
                                        const char *auth_file,
                                        const char *dc_id_text,
                                        const char *password_file,
                                        FILE *stream)
{
    char api_id[32];
    int rc;
    static const char label[] = "mtproto auth.checkPassword";

    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    rc = tg_mtproto_auth_check_password(host, port, api_id, auth_file,
                                        dc_id_text, password_file, stream);
    return rc;
}

int tg_mtproto_auth_login_wizard_file(const char *host,
                                      const char *port,
                                      const char *dc_id_text,
                                      const char *api_file,
                                      const char *auth_file,
                                      const char *code_hash_file,
                                      FILE *stream)
{
    char api_id[32];
    char phone[64];
    char code[64];
    char password[512];
    const char *current_host;
    const char *current_dc_id_text;
    int rc;
    static const char label[] = "mtproto login wizard";

    if (stream == 0 || host == 0 || port == 0 || dc_id_text == 0 ||
        api_file == 0 || auth_file == 0 || code_hash_file == 0) {
        if (stream != 0) {
            fprintf(stream, "%s: invalid-arguments\n", label);
        }
        return 2;
    }

    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    if (tg_mtproto_prompt_line("Phone number: ", phone, sizeof(phone), 1,
                               stream, label) != 0) {
        return 2;
    }

    current_host = host;
    current_dc_id_text = dc_id_text;
    rc = tg_mtproto_auth_send_code_file(current_host, port,
                                        current_dc_id_text, api_file, phone,
                                        auth_file, code_hash_file, stream);
    if (rc > TG_MTPROTO_PHONE_MIGRATE_RC_BASE) {
        unsigned long migrate_dc;
        const char *migrate_host;
        const char *migrate_dc_text;

        migrate_dc = (unsigned long)(rc - TG_MTPROTO_PHONE_MIGRATE_RC_BASE);
        if (tg_mtproto_production_endpoint_for_dc(migrate_dc, &migrate_host,
                                                 &migrate_dc_text) == 0) {
            fprintf(stream, "Using Telegram DC %s.\n", migrate_dc_text);
            current_host = migrate_host;
            current_dc_id_text = migrate_dc_text;
            rc = tg_mtproto_auth_send_code_file(current_host, port,
                                                current_dc_id_text, api_file,
                                                phone, auth_file,
                                                code_hash_file, stream);
        }
    }
    if (rc != 0) {
        tg_mtproto_secure_zero(phone, sizeof(phone));
        return rc;
    }

    if (tg_mtproto_prompt_line("Code: ", code, sizeof(code), 1,
                               stream, label) != 0) {
        tg_mtproto_secure_zero(phone, sizeof(phone));
        return 2;
    }

    rc = tg_mtproto_auth_sign_in_file(current_host, port, api_file, auth_file,
                                      phone, code_hash_file, code,
                                      current_dc_id_text, stream);
    tg_mtproto_secure_zero(code, sizeof(code));
    if (rc != 0) {
        fprintf(stream, "2FA password required. Input may be visible.\n");
        if (tg_mtproto_prompt_line("2FA password, empty to abort: ",
                                   password, sizeof(password), 0, stream,
                                   label) != 0) {
            tg_mtproto_secure_zero(phone, sizeof(phone));
            return 2;
        }
        if (password[0] == '\0') {
            tg_mtproto_secure_zero(phone, sizeof(phone));
            tg_mtproto_secure_zero(password, sizeof(password));
            fprintf(stream, "%s: aborted\n", label);
            return rc;
        }
        rc = tg_mtproto_auth_check_password_text(current_host, port, api_id,
                                                 auth_file,
                                                 current_dc_id_text,
                                                 password, stream);
        tg_mtproto_secure_zero(password, sizeof(password));
        if (rc != 0) {
            tg_mtproto_secure_zero(phone, sizeof(phone));
            return rc;
        }
    }

    tg_mtproto_secure_zero(phone, sizeof(phone));
    rc = tg_mtproto_auth_status_file(current_host, port, api_file, auth_file,
                                     current_dc_id_text, stream);
    if (rc != 0) {
        return rc;
    }
    fprintf(stream, "Login complete.\n");
    return 0;
}

int tg_mtproto_auth_status(const char *host,
                           const char *port,
                           const char *api_id_text,
                           const char *auth_file,
                           const char *dc_id_text,
                           FILE *stream)
{
    unsigned char query[64];
    unsigned long query_length;
    tg_mtproto_rpc_result result;
    tg_mtproto_tl_writer writer;
    tg_mtproto_user_summary user;
    static const char label[] = "mtproto auth.status";

    if (stream == 0 || host == 0 || port == 0 || api_id_text == 0 ||
        auth_file == 0 || dc_id_text == 0) {
        if (stream != 0) {
            fputs("mtproto auth.status: invalid-arguments\n", stream);
        }
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_users_get_self(&writer) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    query_length = writer.length;
    if (tg_mtproto_send_saved_query(host, port, api_id_text, auth_file,
                                    dc_id_text, query, query_length,
                                    &result, stream, label) != 0) {
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        return 2;
    }
    if (tg_mtproto_parse_user_vector_first(result.result_constructor,
                                           result.result_body,
                                           result.result_body_length,
                                           &user) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: session-state-unknown constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    return 0;
}

int tg_mtproto_auth_status_file(const char *host,
                                const char *port,
                                const char *api_file,
                                const char *auth_file,
                                const char *dc_id_text,
                                FILE *stream)
{
    char api_id[32];
    int rc;
    static const char label[] = "mtproto auth.status";

    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    rc = tg_mtproto_auth_status(host, port, api_id, auth_file, dc_id_text,
                                stream);
    return rc;
}

int tg_mtproto_auth_inspect(const char *auth_file, FILE *stream)
{
    tg_mtproto_session session;
    unsigned char auth_key[TG_MTPROTO_AUTH_KEY_LENGTH];
    unsigned long auth_key_id_hi;
    unsigned long auth_key_id_lo;
    tg_mtproto_session_status status;
    static const char label[] = "mtproto auth.inspect";

    if (stream == 0 || auth_file == 0 || auth_file[0] == '\0') {
        if (stream != 0) {
            fprintf(stream, "%s: invalid-arguments\n", label);
        }
        return 2;
    }

    status = tg_mtproto_session_load_authorization(auth_file, &session,
                                                   auth_key);
    if (status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file-invalid (%s)\n", label,
                tg_mtproto_session_status_name(status));
        return 2;
    }

    tg_mtproto_auth_key_id(auth_key, &auth_key_id_hi, &auth_key_id_lo);
    tg_mtproto_secure_zero(auth_key, sizeof(auth_key));

    fprintf(stream, "%s: file valid\n", label);
    fprintf(stream, "%s: dc_id=%lu\n", label, session.dc_id);
    fprintf(stream, "%s: auth_key=present\n", label);
    if (auth_key_id_hi == session.auth_key_id_hi &&
        auth_key_id_lo == session.auth_key_id_lo) {
        fprintf(stream, "%s: auth_key_id=matches\n", label);
    } else {
        fprintf(stream, "%s: auth_key_id=mismatch\n", label);
        return 2;
    }
    fprintf(stream, "%s: server_salt=present\n", label);
    fprintf(stream, "%s: session_id=present\n", label);
    fprintf(stream, "%s: seq_no=%lu\n", label, session.seq_no);
    if (session.last_msg_id_hi != 0UL || session.last_msg_id_lo != 0UL) {
        fprintf(stream, "%s: last_msg_id=present\n", label);
    } else {
        fprintf(stream, "%s: last_msg_id=none\n", label);
    }
    return 0;
}

int tg_mtproto_auth_check_local_files(const char *api_file,
                                      const char *auth_file,
                                      const char *password_file,
                                      const char *code_hash_file,
                                      FILE *stream)
{
    char api_id[32];
    char api_hash[96];
    char password[256];
    unsigned long password_length;
    tg_mtproto_session session;
    unsigned char auth_key[TG_MTPROTO_AUTH_KEY_LENGTH];
    tg_mtproto_session_status session_status;
    int ok;
    static const char label[] = "mtproto local-files";

    if (stream == 0 || api_file == 0 || auth_file == 0) {
        if (stream != 0) {
            fprintf(stream, "%s: invalid-arguments\n", label);
        }
        return 2;
    }

    ok = 1;
    tg_mtproto_check_secret_file_permissions("api-file", api_file, stream);
    if (tg_mtproto_load_api_credentials(api_file, api_id, sizeof(api_id),
                                        api_hash, sizeof(api_hash),
                                        0, 0) != 0) {
        fprintf(stream, "%s: api-file invalid\n", label);
        ok = 0;
    } else {
        fprintf(stream, "%s: api-file ok\n", label);
    }
    tg_mtproto_secure_zero(api_hash, sizeof(api_hash));

    tg_mtproto_check_secret_file_permissions("auth-file", auth_file, stream);
    session_status = tg_mtproto_session_load_authorization(auth_file, &session,
                                                           auth_key);
    if (session_status != TG_MTPROTO_SESSION_OK) {
        fprintf(stream, "%s: auth-file invalid (%s)\n", label,
                tg_mtproto_session_status_name(session_status));
        ok = 0;
    } else {
        fprintf(stream, "%s: auth-file ok\n", label);
        fprintf(stream, "%s: auth-file dc_id=%lu\n", label, session.dc_id);
    }
    tg_mtproto_secure_zero(auth_key, sizeof(auth_key));

    if (password_file != 0 && password_file[0] != '\0') {
        tg_mtproto_check_secret_file_permissions("password-file",
                                                 password_file, stream);
        if (tg_mtproto_load_password_file(password_file, password,
                                          sizeof(password),
                                          &password_length, 0, 0) != 0) {
            fprintf(stream, "%s: password-file invalid\n", label);
            ok = 0;
        } else {
            fprintf(stream, "%s: password-file ok\n", label);
        }
        tg_mtproto_secure_zero(password, sizeof(password));
    } else {
        fprintf(stream, "%s: password-file skipped\n", label);
    }

    if (code_hash_file != 0 && code_hash_file[0] != '\0') {
        tg_mtproto_check_secret_file_permissions("code-hash-file",
                                                 code_hash_file, stream);
        if (tg_mtproto_check_code_hash_file(code_hash_file, stream,
                                            label) != 0) {
            fprintf(stream, "%s: code-hash-file invalid\n", label);
            ok = 0;
        } else {
            fprintf(stream, "%s: code-hash-file ok\n", label);
        }
    } else {
        fprintf(stream, "%s: code-hash-file skipped\n", label);
    }

    if (!ok) {
        fprintf(stream, "%s: failed\n", label);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
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
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
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

int tg_mtproto_auth_get_dialogs(const char *host,
                                const char *port,
                                const char *api_id_text,
                                const char *auth_file,
                                const char *dc_id_text,
                                const char *limit_text,
                                FILE *stream)
{
    unsigned char query[64];
    unsigned long limit;
    unsigned long i;
    tg_mtproto_dialogs_summary dialogs;
    tg_mtproto_dialog_peer_list peer_list;
    tg_mtproto_rpc_result result;
    tg_mtproto_tl_writer writer;
    static const char label[] = "mtproto messages.getDialogs";

    if (stream == 0 || tg_mtproto_parse_ulong_arg(limit_text, &limit) != 0 ||
        limit == 0UL || limit > 100UL) {
        if (stream != 0) {
            fputs("mtproto messages.getDialogs: invalid-arguments\n", stream);
        }
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_dialogs(&writer, limit) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_saved_query(host, port, api_id_text, auth_file,
                                    dc_id_text, query, writer.length, &result,
                                    stream, label) != 0) {
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        return 2;
    }
    if (tg_mtproto_parse_dialogs_summary(result.result_constructor,
                                         result.result_body,
                                         result.result_body_length,
                                         &dialogs) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: dialogs-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: constructor 0x%08lx\n", label,
            dialogs.constructor);
    fprintf(stream, "%s: dialogs %lu messages %lu chats %lu users %lu\n",
            label, dialogs.dialog_count, dialogs.message_count,
            dialogs.chat_count, dialogs.user_count);
    if (dialogs.is_slice || dialogs.is_not_modified) {
        fprintf(stream, "%s: count %lu\n", label, dialogs.count);
    }
    if (tg_mtproto_parse_dialog_peer_list(result.result_constructor,
                                          result.result_body,
                                          result.result_body_length,
                                          &peer_list) == TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: peer_count %lu\n", label, peer_list.count);
        for (i = 0UL; i < peer_list.count; ++i) {
            fprintf(stream,
                    "%s: peer %lu type %s id 0x%08lx%08lx top %lu unread %lu\n",
                    label, i + 1UL,
                    tg_mtproto_peer_constructor_name(
                        peer_list.peers[i].peer_constructor),
                    peer_list.peers[i].id_hi,
                    peer_list.peers[i].id_lo,
                    peer_list.peers[i].top_message,
                    peer_list.peers[i].unread_count);
        }
        if (peer_list.truncated) {
            fprintf(stream, "%s: peer_list_truncated\n", label);
        }
    } else if (dialogs.dialog_count != 0UL) {
        fprintf(stream, "%s: peer_list_parse_skipped\n", label);
    }
    return 0;
}

int tg_mtproto_auth_get_dialogs_file(const char *host,
                                     const char *port,
                                     const char *api_file,
                                     const char *auth_file,
                                     const char *dc_id_text,
                                     const char *limit_text,
                                     FILE *stream)
{
    char api_id[32];
    int rc;
    static const char label[] = "mtproto messages.getDialogs";

    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    rc = tg_mtproto_auth_get_dialogs(host, port, api_id, auth_file,
                                     dc_id_text, limit_text, stream);
    return rc;
}

static void tg_mtproto_write_cache_text(FILE *stream, const char *text)
{
    unsigned long i;

    if (stream == 0 || text == 0) {
        return;
    }
    for (i = 0UL; text[i] != '\0'; ++i) {
        if (text[i] == '\r' || text[i] == '\n' || text[i] == '\t') {
            fputc(' ', stream);
        } else {
            fputc((unsigned char)text[i], stream);
        }
    }
}

#if defined(__amigaos3__) || defined(__amigaos4__) || defined(__AROS__) || \
    defined(__MORPHOS__) || defined(__MORPHOS)
#define TG_MTPROTO_DISPLAY_LATIN1 1
#else
#define TG_MTPROTO_DISPLAY_LATIN1 0
#endif

#if TG_MTPROTO_DISPLAY_LATIN1
static unsigned long tg_mtproto_utf8_read_codepoint(const char *text,
                                                    unsigned long *index)
{
    const unsigned char *bytes;
    unsigned long i;
    unsigned long cp;

    bytes = (const unsigned char *)text;
    i = *index;
    if (bytes[i] < 0x80U) {
        *index = i + 1UL;
        return bytes[i];
    }
    if ((bytes[i] & 0xe0U) == 0xc0U && bytes[i + 1UL] != '\0' &&
        (bytes[i + 1UL] & 0xc0U) == 0x80U) {
        cp = ((unsigned long)(bytes[i] & 0x1fU) << 6) |
             (unsigned long)(bytes[i + 1UL] & 0x3fU);
        *index = i + 2UL;
        return cp;
    }
    if ((bytes[i] & 0xf0U) == 0xe0U && bytes[i + 1UL] != '\0' &&
        bytes[i + 2UL] != '\0' &&
        (bytes[i + 1UL] & 0xc0U) == 0x80U &&
        (bytes[i + 2UL] & 0xc0U) == 0x80U) {
        cp = ((unsigned long)(bytes[i] & 0x0fU) << 12) |
             ((unsigned long)(bytes[i + 1UL] & 0x3fU) << 6) |
             (unsigned long)(bytes[i + 2UL] & 0x3fU);
        *index = i + 3UL;
        return cp;
    }
    if ((bytes[i] & 0xf8U) == 0xf0U && bytes[i + 1UL] != '\0' &&
        bytes[i + 2UL] != '\0' && bytes[i + 3UL] != '\0' &&
        (bytes[i + 1UL] & 0xc0U) == 0x80U &&
        (bytes[i + 2UL] & 0xc0U) == 0x80U &&
        (bytes[i + 3UL] & 0xc0U) == 0x80U) {
        cp = ((unsigned long)(bytes[i] & 0x07U) << 18) |
             ((unsigned long)(bytes[i + 1UL] & 0x3fU) << 12) |
             ((unsigned long)(bytes[i + 2UL] & 0x3fU) << 6) |
             (unsigned long)(bytes[i + 3UL] & 0x3fU);
        *index = i + 4UL;
        return cp;
    }
    *index = i + 1UL;
    return bytes[i];
}

static void tg_mtproto_print_display_codepoint(FILE *stream, unsigned long cp)
{
    if (cp == '\r' || cp == '\n' || cp == '\t') {
        fputc(' ', stream);
    } else if (cp < 0x100UL) {
        fputc((unsigned char)cp, stream);
    } else {
        switch (cp) {
        case 0x2018UL:
        case 0x2019UL:
        case 0x02bcUL:
            fputc('\'', stream);
            break;
        case 0x201cUL:
        case 0x201dUL:
            fputc('"', stream);
            break;
        case 0x2013UL:
        case 0x2014UL:
        case 0x2212UL:
            fputc('-', stream);
            break;
        case 0x2026UL:
            fputs("...", stream);
            break;
        case 0x00a0UL:
            fputc(' ', stream);
            break;
        default:
            fputc('?', stream);
            break;
        }
    }
}
#endif

static void tg_mtproto_print_cache_text(FILE *stream, const char *text)
{
#if TG_MTPROTO_DISPLAY_LATIN1
    unsigned long i;
    unsigned long cp;

    if (stream == 0 || text == 0) {
        return;
    }
    i = 0UL;
    while (text[i] != '\0') {
        cp = tg_mtproto_utf8_read_codepoint(text, &i);
        tg_mtproto_print_display_codepoint(stream, cp);
    }
#else
    tg_mtproto_write_cache_text(stream, text);
#endif
}

static void tg_mtproto_copy_cache_field(char *dest,
                                        unsigned long dest_size,
                                        const char *begin,
                                        const char *end)
{
    unsigned long length;

    if (dest == 0 || dest_size == 0UL) {
        return;
    }
    dest[0] = '\0';
    if (begin == 0) {
        return;
    }
    while (*begin == ' ' || *begin == '\t') {
        ++begin;
    }
    if (end == 0) {
        end = begin + strlen(begin);
    }
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t' ||
                           end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    if (end <= begin || (begin[0] == '-' && begin + 1 == end)) {
        return;
    }
    length = (unsigned long)(end - begin);
    if (length >= dest_size) {
        length = dest_size - 1UL;
    }
    memcpy(dest, begin, (size_t)length);
    dest[length] = '\0';
}

static unsigned long tg_mtproto_peer_constructor_from_name(const char *name)
{
    if (name == 0) {
        return 0UL;
    }
    if (strcmp(name, "user") == 0) {
        return TG_MTPROTO_PEER_USER_CONSTRUCTOR;
    }
    if (strcmp(name, "chat") == 0) {
        return TG_MTPROTO_PEER_CHAT_CONSTRUCTOR;
    }
    if (strcmp(name, "channel") == 0) {
        return TG_MTPROTO_PEER_CHANNEL_CONSTRUCTOR;
    }
    return 0UL;
}

static tg_mtproto_peer_cache_entry *tg_mtproto_peer_cache_find_local(
    tg_mtproto_peer_cache *cache,
    unsigned long peer_constructor,
    unsigned long id_hi,
    unsigned long id_lo)
{
    unsigned long i;

    if (cache == 0) {
        return 0;
    }
    for (i = 0UL; i < cache->count; ++i) {
        if (cache->entries[i].peer_constructor == peer_constructor &&
            cache->entries[i].id_hi == id_hi &&
            cache->entries[i].id_lo == id_lo) {
            return &cache->entries[i];
        }
    }
    return 0;
}

static void tg_mtproto_recount_peer_cache(tg_mtproto_peer_cache *cache)
{
    unsigned long i;

    if (cache == 0) {
        return;
    }
    cache->user_count = 0UL;
    cache->chat_count = 0UL;
    for (i = 0UL; i < cache->count; ++i) {
        if (cache->entries[i].peer_constructor ==
            TG_MTPROTO_PEER_USER_CONSTRUCTOR) {
            ++cache->user_count;
        } else if (cache->entries[i].peer_constructor ==
                       TG_MTPROTO_PEER_CHAT_CONSTRUCTOR ||
                   cache->entries[i].peer_constructor ==
                       TG_MTPROTO_PEER_CHANNEL_CONSTRUCTOR) {
            ++cache->chat_count;
        }
    }
}

static void tg_mtproto_merge_peer_cache_entry(
    tg_mtproto_peer_cache_entry *dest,
    const tg_mtproto_peer_cache_entry *src)
{
    char old_title[sizeof(dest->title)];
    char old_username[sizeof(dest->username)];
    unsigned long old_hash_hi;
    unsigned long old_hash_lo;
    int old_has_access_hash;

    if (dest == 0 || src == 0) {
        return;
    }
    strcpy(old_title, dest->title);
    strcpy(old_username, dest->username);
    old_hash_hi = dest->access_hash_hi;
    old_hash_lo = dest->access_hash_lo;
    old_has_access_hash = dest->has_access_hash;
    *dest = *src;
    if (!dest->has_access_hash && old_has_access_hash) {
        dest->has_access_hash = 1;
        dest->access_hash_hi = old_hash_hi;
        dest->access_hash_lo = old_hash_lo;
    }
    if (dest->title[0] == '\0') {
        strcpy(dest->title, old_title);
    }
    if (dest->username[0] == '\0') {
        strcpy(dest->username, old_username);
    }
}

static int tg_mtproto_load_peer_cache_file(const char *path,
                                           tg_mtproto_peer_cache *cache)
{
    FILE *file;
    char line[512];
    char type[24];
    char hash_text[32];
    char self_text[8];
    char bot_text[8];
    char *title;
    char *username;
    tg_mtproto_peer_cache_entry *entry;
    unsigned long peer_index;
    unsigned long peer_constructor;
    unsigned long id_hi;
    unsigned long id_lo;
    unsigned long top_message;
    unsigned long unread_count;

    if (path == 0 || cache == 0) {
        return 2;
    }
    memset(cache, 0, sizeof(*cache));
    file = fopen(path, "r");
    if (file == 0) {
        return 2;
    }
    while (fgets(line, sizeof(line), file) != 0) {
        peer_index = 0UL;
        id_hi = id_lo = top_message = unread_count = 0UL;
        type[0] = hash_text[0] = self_text[0] = bot_text[0] = '\0';
        if (sscanf(line,
                   "peer %lu type %23s id 0x%8lx%8lx access_hash %31s top %lu unread %lu self %7s bot %7s",
                   &peer_index, type, &id_hi, &id_lo, hash_text,
                   &top_message, &unread_count, self_text, bot_text) != 9) {
            continue;
        }
        peer_constructor = tg_mtproto_peer_constructor_from_name(type);
        if (peer_constructor == 0UL) {
            continue;
        }
        if (cache->count >= TG_MTPROTO_PEER_CACHE_MAX) {
            cache->truncated = 1;
            continue;
        }
        entry = &cache->entries[cache->count++];
        memset(entry, 0, sizeof(*entry));
        entry->peer_constructor = peer_constructor;
        entry->id_hi = id_hi;
        entry->id_lo = id_lo;
        entry->top_message = top_message;
        entry->unread_count = unread_count;
        entry->is_self = strcmp(self_text, "yes") == 0;
        entry->is_bot = strcmp(bot_text, "yes") == 0;
        if (hash_text[0] == '0' && hash_text[1] == 'x' &&
            sscanf(hash_text, "0x%8lx%8lx", &entry->access_hash_hi,
                   &entry->access_hash_lo) == 2) {
            entry->has_access_hash = 1;
        }
        title = strstr(line, " title ");
        username = strstr(line, " username ");
        if (username != 0) {
            tg_mtproto_copy_cache_field(entry->username,
                                        sizeof(entry->username),
                                        username + 10, title);
        }
        if (title != 0) {
            tg_mtproto_copy_cache_field(entry->title, sizeof(entry->title),
                                        title + 7, 0);
        }
    }
    fclose(file);
    tg_mtproto_recount_peer_cache(cache);
    return cache->count > 0UL ? 0 : 2;
}

static int tg_mtproto_peer_cache_available(const char *path)
{
    tg_mtproto_peer_cache cache;

    return tg_mtproto_load_peer_cache_file(path, &cache) == 0;
}

static void tg_mtproto_merge_peer_cache(tg_mtproto_peer_cache *dest,
                                        const tg_mtproto_peer_cache *fresh)
{
    unsigned long i;
    tg_mtproto_peer_cache_entry *entry;

    if (dest == 0 || fresh == 0) {
        return;
    }
    if (fresh->total_dialog_count > dest->total_dialog_count) {
        dest->total_dialog_count = fresh->total_dialog_count;
    }
    for (i = 0UL; i < fresh->count; ++i) {
        entry = tg_mtproto_peer_cache_find_local(
            dest, fresh->entries[i].peer_constructor,
            fresh->entries[i].id_hi, fresh->entries[i].id_lo);
        if (entry != 0) {
            tg_mtproto_merge_peer_cache_entry(entry, &fresh->entries[i]);
            continue;
        }
        if (dest->count >= TG_MTPROTO_PEER_CACHE_MAX) {
            dest->truncated = 1;
            continue;
        }
        dest->entries[dest->count++] = fresh->entries[i];
    }
    if (fresh->truncated) {
        dest->truncated = 1;
    }
    tg_mtproto_recount_peer_cache(dest);
}

static int tg_mtproto_save_peer_cache_file(
    const char *path,
    const tg_mtproto_peer_cache *cache,
    FILE *stream,
    const char *label)
{
    FILE *file;
    unsigned long i;
    unsigned long public_index;
    unsigned long public_count;
    unsigned long public_user_count;
    const tg_mtproto_peer_cache_entry *self_entry;
    const tg_mtproto_peer_cache_entry *entry;

    if (path == 0 || cache == 0) {
        return 2;
    }
    file = fopen(path, "w");
    if (file == 0) {
        if (stream != 0) {
            fprintf(stream, "%s: peer-cache-open-failed\n", label);
        }
        return 2;
    }
    public_count = 0UL;
    public_user_count = 0UL;
    self_entry = 0;
    for (i = 0UL; i < cache->count; ++i) {
        entry = &cache->entries[i];
        if (entry->is_self) {
            self_entry = entry;
            continue;
        }
        ++public_count;
        if (entry->peer_constructor == TG_MTPROTO_PEER_USER_CONSTRUCTOR) {
            ++public_user_count;
        }
    }
    fprintf(file, "mtproto-peer-cache-v1\n");
    fprintf(file, "count %lu total_dialogs %lu users %lu chats %lu\n",
            public_count, cache->total_dialog_count, public_user_count,
            cache->chat_count);
    if (self_entry != 0) {
        fprintf(file, "self username ");
        if (self_entry->username[0] != '\0') {
            tg_mtproto_write_cache_text(file, self_entry->username);
        } else {
            fputc('-', file);
        }
        fprintf(file, " title ");
        if (self_entry->title[0] != '\0') {
            tg_mtproto_write_cache_text(file, self_entry->title);
        } else {
            fputc('-', file);
        }
        fputc('\n', file);
    }
    public_index = 1UL;
    for (i = 0UL; i < cache->count; ++i) {
        entry = &cache->entries[i];
        if (entry->is_self) {
            continue;
        }
        fprintf(file,
                "peer %lu type %s id 0x%08lx%08lx access_hash ",
                public_index,
                tg_mtproto_peer_constructor_name(entry->peer_constructor),
                entry->id_hi, entry->id_lo);
        ++public_index;
        if (entry->has_access_hash) {
            fprintf(file, "0x%08lx%08lx", entry->access_hash_hi,
                    entry->access_hash_lo);
        } else {
            fprintf(file, "-");
        }
        fprintf(file, " top %lu unread %lu self %s bot %s username ",
                entry->top_message, entry->unread_count,
                entry->is_self ? "yes" : "no",
                entry->is_bot ? "yes" : "no");
        if (entry->username[0] != '\0') {
            tg_mtproto_write_cache_text(file, entry->username);
        } else {
            fputc('-', file);
        }
        fprintf(file, " title ");
        if (entry->title[0] != '\0') {
            tg_mtproto_write_cache_text(file, entry->title);
        } else {
            fputc('-', file);
        }
        fputc('\n', file);
    }
    if (cache->truncated) {
        fprintf(file, "truncated yes\n");
    }
    if (fclose(file) != 0) {
        if (stream != 0) {
            fprintf(stream, "%s: peer-cache-close-failed\n", label);
        }
        return 2;
    }
    return 0;
}

int tg_mtproto_auth_list_peers_file(const char *host,
                                    const char *port,
                                    const char *api_file,
                                    const char *auth_file,
                                    const char *dc_id_text,
                                    const char *limit_text,
                                    const char *peer_cache_file,
                                    FILE *stream)
{
    unsigned char query[64];
    unsigned long limit;
    unsigned long i;
    char api_id[32];
    tg_mtproto_dialogs_summary dialogs;
    tg_mtproto_peer_cache cache;
    tg_mtproto_peer_cache existing_cache;
    tg_mtproto_rpc_result result;
    tg_mtproto_tl_writer writer;
    static const char label[] = "mtproto list-peers";

    if (stream == 0 || tg_mtproto_parse_ulong_arg(limit_text, &limit) != 0 ||
        limit == 0UL || limit > 100UL || peer_cache_file == 0) {
        if (stream != 0) {
            fputs("mtproto list-peers: invalid-arguments\n", stream);
        }
        return 2;
    }
    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_dialogs(&writer, limit) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_saved_query(host, port, api_id, auth_file,
                                    dc_id_text, query, writer.length, &result,
                                    stream, label) != 0) {
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        return 2;
    }
    if (tg_mtproto_parse_dialogs_summary(result.result_constructor,
                                         result.result_body,
                                         result.result_body_length,
                                         &dialogs) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_dialog_peer_cache(result.result_constructor,
                                           result.result_body,
                                           result.result_body_length,
                                           &cache) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: dialogs-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    if (tg_mtproto_load_peer_cache_file(peer_cache_file, &existing_cache) == 0) {
        tg_mtproto_merge_peer_cache(&existing_cache, &cache);
        cache = existing_cache;
    }
    if (tg_mtproto_save_peer_cache_file(peer_cache_file, &cache, stream,
                                        label) != 0) {
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: constructor 0x%08lx\n", label,
            dialogs.constructor);
    fprintf(stream, "%s: peers %lu total_dialogs %lu users %lu chats %lu\n",
            label, cache.count, cache.total_dialog_count, cache.user_count,
            cache.chat_count);
    for (i = 0UL; i < cache.count; ++i) {
        fprintf(stream, "%s: peer %lu type %s id 0x%08lx%08lx",
                label, i + 1UL,
                tg_mtproto_peer_constructor_name(
                    cache.entries[i].peer_constructor),
                cache.entries[i].id_hi, cache.entries[i].id_lo);
        if (cache.entries[i].title[0] != '\0') {
            fprintf(stream, " title ");
            tg_mtproto_print_cache_text(stream, cache.entries[i].title);
        }
        if (cache.entries[i].username[0] != '\0') {
            fprintf(stream, " username ");
            tg_mtproto_print_cache_text(stream, cache.entries[i].username);
        }
        fprintf(stream, " unread %lu\n", cache.entries[i].unread_count);
    }
    if (cache.truncated) {
        fprintf(stream, "%s: peer_cache_truncated\n", label);
    }
    fprintf(stream, "%s: peer_cache_saved %s\n", label, peer_cache_file);
    return 0;
}

int tg_mtproto_auth_get_history_self(const char *host,
                                     const char *port,
                                     const char *api_id_text,
                                     const char *auth_file,
                                     const char *dc_id_text,
                                     const char *limit_text,
                                     FILE *stream)
{
    unsigned char query[64];
    unsigned long limit;
    tg_mtproto_messages_summary messages;
    tg_mtproto_rpc_result result;
    tg_mtproto_tl_writer writer;
    static const char label[] = "mtproto messages.getHistory(self)";

    if (stream == 0 || tg_mtproto_parse_ulong_arg(limit_text, &limit) != 0 ||
        limit == 0UL || limit > 100UL) {
        if (stream != 0) {
            fputs("mtproto messages.getHistory(self): invalid-arguments\n",
                  stream);
        }
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_history_self(&writer, limit) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_saved_query(host, port, api_id_text, auth_file,
                                    dc_id_text, query, writer.length, &result,
                                    stream, label) != 0) {
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        return 2;
    }
    if (tg_mtproto_parse_messages_summary(result.result_constructor,
                                          result.result_body,
                                          result.result_body_length,
                                          &messages) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: messages-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: constructor 0x%08lx\n", label,
            messages.constructor);
    fprintf(stream, "%s: messages %lu chats %lu users %lu\n", label,
            messages.message_count, messages.chat_count, messages.user_count);
    if (messages.is_slice || messages.is_not_modified ||
        messages.is_channel_messages) {
        fprintf(stream, "%s: count %lu\n", label, messages.count);
    }
    return 0;
}

int tg_mtproto_auth_get_history_self_file(const char *host,
                                          const char *port,
                                          const char *api_file,
                                          const char *auth_file,
                                          const char *dc_id_text,
                                          const char *limit_text,
                                          FILE *stream)
{
    char api_id[32];
    int rc;
    static const char label[] = "mtproto messages.getHistory(self)";

    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0) {
        return 2;
    }
    rc = tg_mtproto_auth_get_history_self(host, port, api_id, auth_file,
                                          dc_id_text, limit_text, stream);
    return rc;
}

static int tg_mtproto_load_peer_cache_peer(const char *path,
                                           const char *peer_index_text,
                                           unsigned long *peer_constructor,
                                           unsigned long *peer_id_hi,
                                           unsigned long *peer_id_lo,
                                           unsigned long *access_hash_hi,
                                           unsigned long *access_hash_lo,
                                           int *has_access_hash,
                                           FILE *stream,
                                           const char *label)
{
    FILE *file;
    char line[512];
    char type[24];
    unsigned long wanted_index;
    unsigned long index;
    unsigned long id_hi;
    unsigned long id_lo;
    unsigned long hash_hi;
    unsigned long hash_lo;
    unsigned long constructor;
    int matched;

    if (path == 0 || peer_constructor == 0 || peer_id_hi == 0 ||
        peer_id_lo == 0 || access_hash_hi == 0 || access_hash_lo == 0 ||
        has_access_hash == 0 ||
        tg_mtproto_parse_ulong_arg(peer_index_text, &wanted_index) != 0 ||
        wanted_index == 0UL) {
        fprintf(stream, "%s: invalid-peer-cache-arguments\n", label);
        return 2;
    }
    file = fopen(path, "r");
    if (file == 0) {
        fprintf(stream, "%s: peer-cache-open-failed\n", label);
        return 2;
    }
    while (fgets(line, sizeof(line), file) != 0) {
        type[0] = '\0';
        index = 0UL;
        id_hi = id_lo = hash_hi = hash_lo = 0UL;
        matched = sscanf(line,
                         "peer %lu type %23s id 0x%8lx%8lx access_hash 0x%8lx%8lx",
                         &index, type, &id_hi, &id_lo, &hash_hi,
                         &hash_lo);
        if (index == wanted_index) {
            fclose(file);
            if (matched < 4) {
                fprintf(stream, "%s: peer-cache-parse-failed\n", label);
                return 2;
            }
            constructor = tg_mtproto_peer_constructor_from_name(type);
            if (constructor == 0UL) {
                fprintf(stream, "%s: peer-cache-type-unsupported\n", label);
                return 2;
            }
            *peer_constructor = constructor;
            *peer_id_hi = id_hi;
            *peer_id_lo = id_lo;
            *access_hash_hi = hash_hi;
            *access_hash_lo = hash_lo;
            *has_access_hash = matched == 6;
            if ((constructor == TG_MTPROTO_PEER_USER_CONSTRUCTOR ||
                 constructor == TG_MTPROTO_PEER_CHANNEL_CONSTRUCTOR) &&
                !*has_access_hash) {
                fprintf(stream, "%s: peer-cache-access-hash-missing\n",
                        label);
                return 2;
            }
            return 0;
        }
    }
    fclose(file);
    fprintf(stream, "%s: peer-cache-index-not-found\n", label);
    return 2;
}

static void tg_mtproto_print_peer_cache_public(const char *path, FILE *stream)
{
    FILE *file;
    char line[512];
    unsigned long index;
    char type[24];
    char *title;
    char *username;
    int printed_single_header;
    int printed_group_header;
    int pass;
    int want_user;
    int is_user;

    file = fopen(path, "r");
    if (file == 0) {
        fprintf(stream, "chat: peer-cache-open-failed\n");
        return;
    }
    printed_single_header = 0;
    printed_group_header = 0;
    for (pass = 0; pass < 2; ++pass) {
        want_user = pass == 0;
        rewind(file);
        while (fgets(line, sizeof(line), file) != 0) {
            index = 0UL;
            type[0] = '\0';
            if (sscanf(line, "peer %lu type %23s", &index, type) < 2) {
                continue;
            }
            is_user = strcmp(type, "user") == 0;
            if (is_user != want_user) {
                continue;
            }
            if (is_user) {
                if (!printed_single_header) {
                    fprintf(stream, "Single chats:\n");
                    printed_single_header = 1;
                }
            } else {
                if (!printed_group_header) {
                    if (printed_single_header) {
                        fprintf(stream, "\n");
                    }
                    fprintf(stream, "Groups and channels:\n");
                    printed_group_header = 1;
                }
            }
            fprintf(stream, "%lu. ", index);
            title = strstr(line, " title ");
            username = strstr(line, " username ");
            if (title != 0) {
                title += 7;
                tg_mtproto_trim_line(title);
                if (title[0] != '-' || title[1] != '\0') {
                    tg_mtproto_print_cache_text(stream, title);
                }
            } else if (username != 0) {
                username += 10;
                title = strstr(username, " title ");
                if (title != 0) {
                    *title = '\0';
                }
                tg_mtproto_trim_line(username);
                if (username[0] != '-' || username[1] != '\0') {
                    fprintf(stream, "@");
                    tg_mtproto_print_cache_text(stream, username);
                }
            } else {
                tg_mtproto_print_cache_text(stream, type);
            }
            fprintf(stream, "\n");
        }
    }
    if (!printed_single_header && !printed_group_header) {
        fprintf(stream, "No chats available.\n");
    }
    fclose(file);
}

static int tg_mtproto_load_self_cache_label(const char *path,
                                            char *label_buffer,
                                            unsigned long label_buffer_size)
{
    FILE *file;
    char line[512];
    char *title;
    char *username;

    if (label_buffer == 0 || label_buffer_size == 0UL) {
        return 2;
    }
    label_buffer[0] = '\0';
    if (path == 0) {
        return 2;
    }
    file = fopen(path, "r");
    if (file == 0) {
        return 2;
    }
    while (fgets(line, sizeof(line), file) != 0) {
        if (strncmp(line, "self ", 5) != 0) {
            continue;
        }
        title = strstr(line, " title ");
        username = strstr(line, " username ");
        if (title != 0) {
            tg_mtproto_copy_cache_field(label_buffer, label_buffer_size,
                                        title + 7, 0);
        }
        if (label_buffer[0] == '\0' && username != 0) {
            tg_mtproto_copy_cache_field(label_buffer, label_buffer_size,
                                        username + 10, title);
        }
        fclose(file);
        return label_buffer[0] != '\0' ? 0 : 2;
    }
    fclose(file);
    return 2;
}

static int tg_mtproto_load_peer_cache_label(const char *path,
                                            const char *peer_index_text,
                                            char *label_buffer,
                                            unsigned long label_buffer_size)
{
    FILE *file;
    char line[512];
    char type[24];
    unsigned long wanted_index;
    unsigned long index;
    char *title;
    char *username;

    if (label_buffer == 0 || label_buffer_size == 0UL) {
        return 2;
    }
    label_buffer[0] = '\0';
    if (path == 0 || peer_index_text == 0 ||
        tg_mtproto_parse_ulong_arg(peer_index_text, &wanted_index) != 0 ||
        wanted_index == 0UL) {
        return 2;
    }
    file = fopen(path, "r");
    if (file == 0) {
        return 2;
    }
    while (fgets(line, sizeof(line), file) != 0) {
        index = 0UL;
        type[0] = '\0';
        if (sscanf(line, "peer %lu type %23s", &index, type) < 2 ||
            index != wanted_index) {
            continue;
        }
        title = strstr(line, " title ");
        username = strstr(line, " username ");
        if (title != 0) {
            tg_mtproto_copy_cache_field(label_buffer, label_buffer_size,
                                        title + 7, 0);
        }
        if (label_buffer[0] == '\0' && username != 0) {
            tg_mtproto_copy_cache_field(label_buffer, label_buffer_size,
                                        username + 10, title);
        }
        fclose(file);
        return label_buffer[0] != '\0' ? 0 : 2;
    }
    fclose(file);
    return 2;
}

int tg_mtproto_auth_get_history_peer_file(const char *host,
                                          const char *port,
                                          const char *api_file,
                                          const char *auth_file,
                                          const char *dc_id_text,
                                          const char *peer_cache_file,
                                          const char *peer_index_text,
                                          const char *limit_text,
                                          FILE *stream)
{
    unsigned char query[64];
    unsigned long limit;
    unsigned long peer_constructor;
    unsigned long peer_id_hi;
    unsigned long peer_id_lo;
    unsigned long access_hash_hi;
    unsigned long access_hash_lo;
    int has_access_hash;
    char api_id[32];
    tg_mtproto_messages_summary messages;
    tg_mtproto_rpc_result result;
    tg_mtproto_tl_writer writer;
    static const char label[] = "mtproto messages.getHistory(peer)";

    if (stream == 0 || tg_mtproto_parse_ulong_arg(limit_text, &limit) != 0 ||
        limit == 0UL || limit > 100UL) {
        if (stream != 0) {
            fputs("mtproto messages.getHistory(peer): invalid-arguments\n",
                  stream);
        }
        return 2;
    }
    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0 ||
        tg_mtproto_load_peer_cache_peer(peer_cache_file, peer_index_text,
                                        &peer_constructor, &peer_id_hi,
                                        &peer_id_lo, &access_hash_hi,
                                        &access_hash_lo, &has_access_hash,
                                        stream, label) != 0) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_history_peer(
            &writer, peer_constructor, peer_id_hi, peer_id_lo,
            access_hash_hi, access_hash_lo, has_access_hash, limit) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_saved_query(host, port, api_id, auth_file, dc_id_text,
                                    query, writer.length, &result, stream,
                                    label) != 0) {
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        return 2;
    }
    if (tg_mtproto_parse_messages_summary(result.result_constructor,
                                          result.result_body,
                                          result.result_body_length,
                                          &messages) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: messages-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: constructor 0x%08lx\n", label,
            messages.constructor);
    fprintf(stream, "%s: messages %lu chats %lu users %lu\n", label,
            messages.message_count, messages.chat_count, messages.user_count);
    if (messages.is_slice || messages.is_not_modified ||
        messages.is_channel_messages) {
        fprintf(stream, "%s: count %lu\n", label, messages.count);
    }
    return 0;
}

int tg_mtproto_auth_send_self(const char *host,
                              const char *port,
                              const char *api_id_text,
                              const char *auth_file,
                              const char *dc_id_text,
                              const char *message,
                              FILE *stream)
{
    unsigned char query[512];
    unsigned char random_id[8];
    unsigned long random_id_hi;
    unsigned long random_id_lo;
    tg_mtproto_rpc_result result;
    tg_mtproto_tl_writer writer;
    tg_mtproto_updates_summary updates;
    static const char label[] = "mtproto messages.sendMessage(self)";

    if (stream == 0 || message == 0 || message[0] == '\0') {
        if (stream != 0) {
            fputs("mtproto messages.sendMessage(self): invalid-arguments\n",
                  stream);
        }
        return 2;
    }
    tg_mtproto_saved_session_random(random_id, sizeof(random_id));
    random_id_lo = tg_mtproto_read_u32_le(random_id);
    random_id_hi = tg_mtproto_read_u32_le(random_id + 4U);

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_send_self(&writer, message, random_id_hi,
                                            random_id_lo) !=
        TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_saved_query(host, port, api_id_text, auth_file,
                                    dc_id_text, query, writer.length, &result,
                                    stream, label) != 0) {
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        return 2;
    }
    if (tg_mtproto_parse_updates_summary(result.result_constructor,
                                         result.result_body,
                                         result.result_body_length,
                                         &updates) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: updates-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: constructor 0x%08lx\n", label,
            updates.constructor);
    if (updates.has_sent_message) {
        fprintf(stream, "%s: message_id %lu date %lu\n", label, updates.id,
                updates.date);
    }
    return 0;
}

int tg_mtproto_auth_send_peer_file(const char *host,
                                   const char *port,
                                   const char *api_file,
                                   const char *auth_file,
                                   const char *dc_id_text,
                                   const char *peer_cache_file,
                                   const char *peer_index_text,
                                   const char *message,
                                   FILE *stream)
{
    unsigned char query[512];
    unsigned char random_id[8];
    unsigned long random_id_hi;
    unsigned long random_id_lo;
    unsigned long peer_constructor;
    unsigned long peer_id_hi;
    unsigned long peer_id_lo;
    unsigned long access_hash_hi;
    unsigned long access_hash_lo;
    int has_access_hash;
    char api_id[32];
    tg_mtproto_rpc_result result;
    tg_mtproto_tl_writer writer;
    tg_mtproto_updates_summary updates;
    static const char label[] = "mtproto messages.sendMessage(peer)";

    if (stream == 0 || message == 0 || message[0] == '\0') {
        if (stream != 0) {
            fputs("mtproto messages.sendMessage(peer): invalid-arguments\n",
                  stream);
        }
        return 2;
    }
    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    stream, label) != 0 ||
        tg_mtproto_load_peer_cache_peer(peer_cache_file, peer_index_text,
                                        &peer_constructor, &peer_id_hi,
                                        &peer_id_lo, &access_hash_hi,
                                        &access_hash_lo, &has_access_hash,
                                        stream, label) != 0) {
        return 2;
    }
    tg_mtproto_saved_session_random(random_id, sizeof(random_id));
    random_id_lo = tg_mtproto_read_u32_le(random_id);
    random_id_hi = tg_mtproto_read_u32_le(random_id + 4U);

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_send_peer(
            &writer, peer_constructor, peer_id_hi, peer_id_lo,
            access_hash_hi, access_hash_lo, has_access_hash, message,
            random_id_hi, random_id_lo) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: query-build-failed\n", label);
        return 2;
    }
    if (tg_mtproto_send_saved_query(host, port, api_id, auth_file, dc_id_text,
                                    query, writer.length, &result, stream,
                                    label) != 0) {
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        if (!tg_mtproto_print_rpc_error(label, &result, stream)) {
            fprintf(stream, "%s: rpc-error-parse-failed\n", label);
        }
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, stream, label) != 0) {
        return 2;
    }
    if (tg_mtproto_parse_updates_summary(result.result_constructor,
                                         result.result_body,
                                         result.result_body_length,
                                         &updates) != TG_MTPROTO_TL_OK) {
        fprintf(stream, "%s: updates-parse-failed constructor 0x%08lx\n",
                label, result.result_constructor);
        return 2;
    }
    fprintf(stream, "%s: ok\n", label);
    fprintf(stream, "%s: constructor 0x%08lx\n", label,
            updates.constructor);
    if (updates.has_sent_message) {
        fprintf(stream, "%s: message_id %lu date %lu\n", label, updates.id,
                updates.date);
    }
    return 0;
}

static FILE *tg_mtproto_open_quiet_stream(FILE *fallback)
{
    FILE *quiet;

    quiet = tmpfile();
    if (quiet == 0) {
        return fallback;
    }
    return quiet;
}

static void tg_mtproto_close_quiet_stream(FILE *quiet, FILE *fallback)
{
    if (quiet != 0 && quiet != fallback) {
        fclose(quiet);
    }
}

static void tg_mtproto_replay_quiet_stream(FILE *quiet, FILE *fallback)
{
    char line[256];

    if (quiet == 0 || fallback == 0 || quiet == fallback) {
        return;
    }
    rewind(quiet);
    while (fgets(line, sizeof(line), quiet) != 0) {
        fputs(line, fallback);
    }
}

static int tg_mtproto_quiet_stream_has_output(FILE *quiet, FILE *fallback)
{
    long position;

    if (quiet == 0 || quiet == fallback) {
        return 1;
    }
    position = ftell(quiet);
    return position > 0L;
}

static int tg_mtproto_chat_read_line(char *line,
                                     unsigned long line_size,
                                     unsigned long *line_length,
                                     unsigned long timeout_seconds)
{
    char ch;
    int rc;

    if (line == 0 || line_size == 0 || line_length == 0) {
        return -1;
    }
    rc = tg_platform_stdin_read_char(timeout_seconds, &ch);
    if (rc <= 0) {
        return rc;
    }
    if (ch == '\r' || ch == '\n') {
        if (*line_length >= line_size) {
            *line_length = line_size - 1UL;
        }
        line[*line_length] = '\0';
        *line_length = 0UL;
        return 1;
    }
    if (ch == '\b' || ch == 127) {
        if (*line_length > 0UL) {
            --(*line_length);
        }
        return 0;
    }
    if (*line_length + 1UL < line_size) {
        line[*line_length] = ch;
        ++(*line_length);
    }
    return 0;
}

static int tg_mtproto_chat_prompt_line(const char *prompt,
                                       char *out,
                                       unsigned long out_size,
                                       int required,
                                       FILE *stream,
                                       const char *label)
{
    unsigned long line_length;
    int rc;

    if (out != 0 && out_size > 0UL) {
        out[0] = '\0';
    }
    if (prompt == 0 || out == 0 || out_size == 0UL || stream == 0 ||
        label == 0) {
        return 2;
    }
    fputs(prompt, stream);
    fflush(stream);
    line_length = 0UL;
    for (;;) {
        rc = tg_mtproto_chat_read_line(out, out_size, &line_length, 3600UL);
        if (rc < 0) {
            fprintf(stream, "%s: input-closed\n", label);
            return 2;
        }
        if (rc == 0) {
            continue;
        }
        tg_mtproto_trim_line(out);
        if (required && out[0] == '\0') {
            fprintf(stream, "%s: input-empty\n", label);
            return 2;
        }
        return 0;
    }
}

static int tg_mtproto_chat_peer_command_arg(const char *line,
                                            const char **arg)
{
    const char *p;

    if (line == 0 || arg == 0) {
        return 0;
    }
    if (strncmp(line, "/peer", 5) == 0 &&
        (line[5] == '\0' || line[5] == ' ' || line[5] == '\t')) {
        p = line + 5;
    } else if (strncmp(line, "peer", 4) == 0 &&
               (line[4] == '\0' || line[4] == ' ' || line[4] == '\t')) {
        p = line + 4;
    } else {
        return 0;
    }
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    *arg = p;
    return 1;
}

static int tg_mtproto_chat_copy_peer_index(char *dest,
                                           unsigned long dest_size,
                                           const char *src)
{
    const char *peer_arg;
    unsigned long length;

    if (dest == 0 || dest_size == 0UL || src == 0 || src[0] == '\0') {
        return 2;
    }
    if (tg_mtproto_chat_peer_command_arg(src, &peer_arg) &&
        peer_arg[0] != '\0') {
        src = peer_arg;
    }
    length = (unsigned long)strlen(src);
    if (length >= dest_size) {
        return 2;
    }
    strcpy(dest, src);
    tg_mtproto_trim_line(dest);
    return dest[0] != '\0' ? 0 : 2;
}

static int tg_mtproto_auth_print_history_text_peer_file(
    const char *host,
    const char *port,
    const char *api_file,
    const char *auth_file,
    const char *dc_id_text,
    const char *peer_cache_file,
    const char *peer_index_text,
    const char *limit_text,
    FILE *stream,
    unsigned long *last_seen_message_id,
    int only_new,
    int include_outgoing,
    int print_empty_status,
    const char *peer_label,
    const char *own_label)
{
    unsigned char query[64];
    unsigned long limit;
    unsigned long peer_constructor;
    unsigned long peer_id_hi;
    unsigned long peer_id_lo;
    unsigned long access_hash_hi;
    unsigned long access_hash_lo;
    int has_access_hash;
    unsigned long i;
    unsigned long max_seen_message_id;
    unsigned long printed;
    char api_id[32];
    FILE *quiet;
    tg_mtproto_message_text_list texts;
    tg_mtproto_rpc_result result;
    tg_mtproto_tl_writer writer;
    static const char label[] = "mtproto messages.getHistory(peer)";

    if (stream == 0 || tg_mtproto_parse_ulong_arg(limit_text, &limit) != 0 ||
        limit == 0UL || limit > 100UL) {
        return 2;
    }

    quiet = tg_mtproto_open_quiet_stream(stream);
    if (tg_mtproto_load_api_id_file(api_file, api_id, sizeof(api_id),
                                    quiet, label) != 0 ||
        tg_mtproto_load_peer_cache_peer(peer_cache_file, peer_index_text,
                                        &peer_constructor, &peer_id_hi,
                                        &peer_id_lo, &access_hash_hi,
                                        &access_hash_lo, &has_access_hash,
                                        quiet, label) != 0) {
        tg_mtproto_close_quiet_stream(quiet, stream);
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_history_peer(
            &writer, peer_constructor, peer_id_hi, peer_id_lo,
            access_hash_hi, access_hash_lo, has_access_hash, limit) !=
        TG_MTPROTO_TL_OK) {
        tg_mtproto_close_quiet_stream(quiet, stream);
        return 2;
    }
    if (tg_mtproto_send_saved_query(host, port, api_id, auth_file, dc_id_text,
                                    query, writer.length, &result, quiet,
                                    label) != 0) {
        tg_mtproto_close_quiet_stream(quiet, stream);
        return 2;
    }
    if (result.result_constructor == TG_MTPROTO_RPC_ERROR_CONSTRUCTOR) {
        (void)tg_mtproto_print_rpc_error(label, &result, quiet);
        tg_mtproto_close_quiet_stream(quiet, stream);
        return 2;
    }
    if (tg_mtproto_unpack_gzip_result(&result, quiet, label) != 0) {
        tg_mtproto_close_quiet_stream(quiet, stream);
        return 2;
    }
    if (tg_mtproto_parse_message_text_list(result.result_constructor,
                                           result.result_body,
                                           result.result_body_length,
                                           &texts) != TG_MTPROTO_TL_OK) {
        tg_mtproto_close_quiet_stream(quiet, stream);
        return 2;
    }
    tg_mtproto_close_quiet_stream(quiet, stream);

    max_seen_message_id = last_seen_message_id != 0 ?
        *last_seen_message_id : 0UL;
    printed = 0UL;
    if (texts.count == 0UL) {
        if (print_empty_status) {
            fprintf(stream, "history refreshed\n");
        }
        return 0;
    }
    i = texts.count;
    while (i > 0UL) {
        --i;
        if (texts.messages[i].id > max_seen_message_id) {
            max_seen_message_id = texts.messages[i].id;
        }
        if (last_seen_message_id != 0 && only_new &&
            texts.messages[i].id <= *last_seen_message_id) {
            continue;
        }
        if (!include_outgoing && texts.messages[i].is_out) {
            continue;
        }
        if (texts.messages[i].is_out) {
            if (own_label != 0 && own_label[0] != '\0') {
                tg_mtproto_print_cache_text(stream, own_label);
                fprintf(stream, ": ");
            } else {
                fprintf(stream, "me: ");
            }
        } else if (peer_label != 0 && peer_label[0] != '\0') {
            tg_mtproto_print_cache_text(stream, peer_label);
            fprintf(stream, ": ");
        } else {
            fprintf(stream, "them: ");
        }
        tg_mtproto_print_cache_text(stream, texts.messages[i].text);
        fprintf(stream, "\n");
        ++printed;
    }
    if (last_seen_message_id != 0) {
        *last_seen_message_id = max_seen_message_id;
    }
    if (printed == 0UL && print_empty_status) {
        fprintf(stream, "history refreshed\n");
    }
    return 0;
}

static void tg_mtproto_chat_print_input_prompt(FILE *stream,
                                               const char *own_label)
{
    if (stream == 0) {
        return;
    }
    if (own_label != 0 && own_label[0] != '\0') {
        tg_mtproto_print_cache_text(stream, own_label);
    } else {
        fprintf(stream, "me");
    }
    fprintf(stream, ": ");
    fflush(stream);
}

static void tg_mtproto_chat_print_help(FILE *stream)
{
    if (stream == 0) {
        return;
    }
    fprintf(stream, "\nCommands:\n");
    fprintf(stream, "  <text>        send a message to the selected chat\n");
    fprintf(stream, "  Enter         read new messages now\n");
    fprintf(stream, "  /read         read recent messages\n");
    fprintf(stream, "  /peer         choose another chat\n");
    fprintf(stream, "  /peer <n>     switch directly to chat number n\n");
    fprintf(stream, "  /peers        refresh the chat list\n");
    fprintf(stream, "  /watch <sec>  set auto-read interval\n");
    fprintf(stream, "  /watch off    disable auto-read\n");
    fprintf(stream, "  /help         show this help\n");
    fprintf(stream, "  /quit         exit\n\n");
}

int tg_mtproto_auth_chat_file(const char *host,
                              const char *port,
                              const char *api_file,
                              const char *auth_file,
                              const char *dc_id_text,
                              const char *peer_cache_file,
                              FILE *stream)
{
    char peer_index[32];
    char peer_label[128];
    char requested_peer_text[32];
    char requested_peer_index[32];
    char requested_peer_label[128];
    char own_label[128];
    char line[512];
    const char *peer_arg;
    unsigned long line_length;
    unsigned long last_seen_message_id;
    unsigned long requested_last_seen_message_id;
    unsigned long watch_seconds;
    unsigned long parsed_watch_seconds;
    FILE *quiet;
    int rc;
    static const char label[] = "chat";

    if (stream == 0 || host == 0 || port == 0 || api_file == 0 ||
        auth_file == 0 || dc_id_text == 0 || peer_cache_file == 0) {
        if (stream != 0) {
            fputs("chat: invalid-arguments\n", stream);
        }
        return 2;
    }
    quiet = tg_mtproto_open_quiet_stream(stream);
    rc = tg_mtproto_auth_list_peers_file(host, port, api_file, auth_file,
                                         dc_id_text, "100", peer_cache_file,
                                         quiet);
    tg_mtproto_close_quiet_stream(quiet, stream);
    if (rc != 0 && !tg_mtproto_peer_cache_available(peer_cache_file)) {
        fprintf(stream, "%s: list-peers-failed\n", label);
        return 2;
    }
    if (rc != 0) {
        fprintf(stream, "%s: using cached peers\n", label);
    }
    if (tg_mtproto_load_self_cache_label(peer_cache_file, own_label,
                                         sizeof(own_label)) != 0) {
        strcpy(own_label, "me");
    }
    fprintf(stream, "Choose a chat:\n\n");
    tg_mtproto_print_peer_cache_public(peer_cache_file, stream);
    if (tg_mtproto_chat_prompt_line("\nPeer index: ", peer_index,
                                    sizeof(peer_index), 1, stream,
                                    label) != 0) {
        return 2;
    }
    if (tg_mtproto_load_peer_cache_label(peer_cache_file, peer_index,
                                         peer_label,
                                         sizeof(peer_label)) != 0) {
        peer_label[0] = '\0';
    }
    last_seen_message_id = 0UL;
    watch_seconds = 2UL;
    rc = tg_mtproto_auth_print_history_text_peer_file(
        host, port, api_file, auth_file, dc_id_text, peer_cache_file,
        peer_index, "5", stream, &last_seen_message_id, 0, 1, 1,
        peer_label, own_label);
    if (rc != 0) {
        fprintf(stream, "%s: read-failed\n", label);
    }
    line_length = 0UL;
    tg_mtproto_chat_print_help(stream);
    fprintf(stream, "%s: auto-read every %lu second(s)\n", label,
            watch_seconds);
    tg_mtproto_chat_print_input_prompt(stream, own_label);
    for (;;) {
        if (watch_seconds == 0UL) {
            rc = tg_mtproto_chat_read_line(line, sizeof(line), &line_length,
                                           3600UL);
            if (rc == 0) {
                continue;
            }
        } else {
            rc = tg_mtproto_chat_read_line(line, sizeof(line), &line_length,
                                           watch_seconds);
        }
        if (rc == 0) {
            if (line_length > 0UL) {
                continue;
            }
            quiet = tg_mtproto_open_quiet_stream(stream);
            rc = tg_mtproto_auth_print_history_text_peer_file(
                host, port, api_file, auth_file, dc_id_text,
                peer_cache_file, peer_index, "5", quiet,
                &last_seen_message_id, 1, 0, 0, peer_label, own_label);
            if (rc == 0 && tg_mtproto_quiet_stream_has_output(quiet, stream)) {
                fprintf(stream, "\n");
                tg_mtproto_replay_quiet_stream(quiet, stream);
                tg_mtproto_close_quiet_stream(quiet, stream);
                tg_mtproto_chat_print_input_prompt(stream, own_label);
                continue;
            }
            tg_mtproto_close_quiet_stream(quiet, stream);
            continue;
        }
        if (rc < 0) {
            fprintf(stream, "%s: input-closed\n", label);
            return 0;
        }
        tg_mtproto_trim_line(line);
        if (line[0] == '\0') {
            rc = tg_mtproto_auth_print_history_text_peer_file(
                host, port, api_file, auth_file, dc_id_text,
                peer_cache_file, peer_index, "5", stream,
                &last_seen_message_id, 1, 0, 0, peer_label, own_label);
            if (rc != 0) {
                fprintf(stream, "%s: read-failed\n", label);
            }
            tg_mtproto_chat_print_input_prompt(stream, own_label);
            continue;
        }
        if (strcmp(line, "/quit") == 0 || strcmp(line, "quit") == 0) {
            fprintf(stream, "%s: bye\n", label);
            return 0;
        }
        if (strcmp(line, "/help") == 0 || strcmp(line, "help") == 0) {
            tg_mtproto_chat_print_help(stream);
            tg_mtproto_chat_print_input_prompt(stream, own_label);
            continue;
        }
        if (strcmp(line, "/peers") == 0) {
            quiet = tg_mtproto_open_quiet_stream(stream);
            rc = tg_mtproto_auth_list_peers_file(host, port, api_file,
                                                 auth_file, dc_id_text, "100",
                                                 peer_cache_file, quiet);
            tg_mtproto_close_quiet_stream(quiet, stream);
            if (rc != 0 && !tg_mtproto_peer_cache_available(peer_cache_file)) {
                fprintf(stream, "%s: list-peers-failed\n", label);
                return 2;
            }
            if (rc != 0) {
                fprintf(stream, "%s: using cached peers\n", label);
            }
            if (tg_mtproto_load_self_cache_label(peer_cache_file, own_label,
                                                 sizeof(own_label)) != 0) {
                strcpy(own_label, "me");
            }
            fprintf(stream, "\nChoose a chat:\n\n");
            tg_mtproto_print_peer_cache_public(peer_cache_file, stream);
            tg_mtproto_chat_print_input_prompt(stream, own_label);
            continue;
        }
        if (tg_mtproto_chat_peer_command_arg(line, &peer_arg)) {
            if (peer_arg[0] == '\0') {
                fprintf(stream, "\nChoose a chat:\n\n");
                tg_mtproto_print_peer_cache_public(peer_cache_file, stream);
                if (tg_mtproto_chat_prompt_line("Peer index: ",
                                                requested_peer_text,
                                                sizeof(requested_peer_text),
                                                1, stream, label) != 0) {
                    return 2;
                }
                if (tg_mtproto_chat_copy_peer_index(
                        requested_peer_index, sizeof(requested_peer_index),
                        requested_peer_text) != 0) {
                    fprintf(stream, "%s: use /peer <number>\n", label);
                    tg_mtproto_chat_print_input_prompt(stream, own_label);
                    continue;
                }
            } else if (tg_mtproto_chat_copy_peer_index(
                           requested_peer_index,
                           sizeof(requested_peer_index), peer_arg) != 0) {
                fprintf(stream, "%s: use /peer <number>\n", label);
                tg_mtproto_chat_print_input_prompt(stream, own_label);
                continue;
            }
            if (tg_mtproto_load_peer_cache_label(peer_cache_file,
                                                 requested_peer_index,
                                                 requested_peer_label,
                                                 sizeof(requested_peer_label))
                != 0) {
                fprintf(stream, "%s: peer-not-found\n", label);
                tg_mtproto_chat_print_input_prompt(stream, own_label);
                continue;
            }
            strcpy(peer_index, requested_peer_index);
            strcpy(peer_label, requested_peer_label);
            last_seen_message_id = 0UL;
            fprintf(stream, "Selected: ");
            tg_mtproto_print_cache_text(stream, peer_label);
            fprintf(stream, "\n");
            requested_last_seen_message_id = 0UL;
            rc = tg_mtproto_auth_print_history_text_peer_file(
                host, port, api_file, auth_file, dc_id_text,
                peer_cache_file, requested_peer_index, "5", stream,
                &requested_last_seen_message_id, 0, 1, 1,
                requested_peer_label, own_label);
            if (rc != 0) {
                fprintf(stream, "%s: read-failed\n", label);
                tg_mtproto_chat_print_input_prompt(stream, own_label);
                continue;
            }
            last_seen_message_id = requested_last_seen_message_id;
            tg_mtproto_chat_print_input_prompt(stream, own_label);
            continue;
        }
        if (strcmp(line, "/read") == 0) {
            rc = tg_mtproto_auth_print_history_text_peer_file(
                host, port, api_file, auth_file, dc_id_text,
                peer_cache_file, peer_index, "5", stream,
                &last_seen_message_id, 0, 1, 1, peer_label, own_label);
            if (rc != 0) {
                fprintf(stream, "%s: read-failed\n", label);
            }
            tg_mtproto_chat_print_input_prompt(stream, own_label);
            continue;
        }
        if ((strncmp(line, "/watch", 6) == 0 &&
             (line[6] == '\0' || line[6] == ' ' || line[6] == '\t')) ||
            (strncmp(line, "watch", 5) == 0 &&
             (line[5] == '\0' || line[5] == ' ' || line[5] == '\t'))) {
            if (tg_console_parse_watch_command(line, &parsed_watch_seconds) !=
                0) {
                fprintf(stream,
                        "%s: use /watch <seconds <= 3600> or /watch off\n",
                        label);
                continue;
            }
            watch_seconds = parsed_watch_seconds;
            if (watch_seconds == 0UL) {
                fprintf(stream, "%s: auto-read disabled\n", label);
            } else {
                fprintf(stream, "%s: auto-read every %lu second(s)\n", label,
                        watch_seconds);
            }
            tg_mtproto_chat_print_input_prompt(stream, own_label);
            continue;
        }
        quiet = tg_mtproto_open_quiet_stream(stream);
        rc = tg_mtproto_auth_send_peer_file(host, port, api_file, auth_file,
                                            dc_id_text, peer_cache_file,
                                            peer_index, line, quiet);
        if (rc != 0) {
            tg_mtproto_replay_quiet_stream(quiet, stream);
            tg_mtproto_close_quiet_stream(quiet, stream);
            fprintf(stream, "%s: send-failed\n", label);
            tg_mtproto_chat_print_input_prompt(stream, own_label);
            continue;
        }
        tg_mtproto_close_quiet_stream(quiet, stream);
        tg_mtproto_chat_print_input_prompt(stream, own_label);
    }
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
    unsigned int i;
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
    for (i = 0U; i < res_pq.fingerprint_count; ++i) {
        fprintf(stream, "mtproto req_pq probe: fingerprint[%u] 0x%08lx%08lx\n",
                i, res_pq.fingerprints[i].hi, res_pq.fingerprints[i].lo);
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
    static const char password_path[] = "telegram-mtproto-password-self-test.tmp";
    static const char missing_password_path[] =
        "telegram-mtproto-password-missing-self-test.tmp";
    static const char password_text[] = "secret\r\n";
    static const char api_path[] = "telegram-mtproto-api-self-test.tmp";
    static const char missing_api_path[] =
        "telegram-mtproto-api-missing-self-test.tmp";
    static const char api_text[] = "\n 12345 \r\n abcdef0123456789 \n";
    static const char peer_path[] = "telegram-mtproto-peer-self-test.tmp";
    static const char peer_text[] =
        "mtproto-peer-cache-v1\n"
        "count 3 total_dialogs 3 users 1 chats 2\n"
        "peer 1 type user id 0x0000000000000001 access_hash 0x0000000000000002 top 0 unread 0 self no bot no username ada title Ada\n"
        "peer 2 type chat id 0x0000000000000003 access_hash - top 0 unread 0 self no bot no username - title Test Group\n"
        "peer 3 type channel id 0x0000000000000004 access_hash 0x0000000000000005 top 0 unread 0 self no bot no username - title Test Channel\n";
    unsigned char payload[64];
    unsigned char packet[80];
    char api_id[32];
    char api_hash[96];
    char password[16];
    unsigned long password_length;
    unsigned long peer_constructor;
    unsigned long peer_id_hi;
    unsigned long peer_id_lo;
    unsigned long access_hash_hi;
    unsigned long access_hash_lo;
    int has_access_hash;
    FILE *quiet;
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

    (void)remove(password_path);
    (void)remove(missing_password_path);
    if (tg_mtproto_load_password_file(missing_password_path, password,
                                      sizeof(password), &password_length,
                                      0, 0) == 0) {
        return 2;
    }
    if (tg_file_write_text(password_path, "", 0UL) != TG_FILE_OK ||
        tg_mtproto_load_password_file(password_path, password,
                                      sizeof(password), &password_length,
                                      0, 0) == 0) {
        (void)remove(password_path);
        return 2;
    }
    if (tg_file_write_text(password_path, password_text,
                           (unsigned long)strlen(password_text)) !=
            TG_FILE_OK ||
        tg_mtproto_load_password_file(password_path, password,
                                      sizeof(password), &password_length,
                                      0, 0) != 0 ||
        password_length != 6UL ||
        strcmp(password, "secret") != 0) {
        (void)remove(password_path);
        return 2;
    }
    tg_mtproto_secure_zero(password, sizeof(password));
    if (tg_mtproto_load_password_file(password_path, password, 4UL,
                                      &password_length, 0, 0) == 0) {
        (void)remove(password_path);
        return 2;
    }
    (void)remove(password_path);

    (void)remove(api_path);
    (void)remove(missing_api_path);
    if (tg_mtproto_load_api_credentials(missing_api_path, api_id,
                                        sizeof(api_id), api_hash,
                                        sizeof(api_hash), 0, 0) == 0) {
        return 2;
    }
    if (tg_file_write_text(api_path, "12345\n", 6UL) != TG_FILE_OK ||
        tg_mtproto_load_api_credentials(api_path, api_id, sizeof(api_id),
                                        api_hash, sizeof(api_hash),
                                        0, 0) == 0) {
        (void)remove(api_path);
        return 2;
    }
    if (tg_file_write_text(api_path, api_text,
                           (unsigned long)strlen(api_text)) != TG_FILE_OK ||
        tg_mtproto_load_api_credentials(api_path, api_id, sizeof(api_id),
                                        api_hash, sizeof(api_hash),
                                        0, 0) != 0 ||
        strcmp(api_id, "12345") != 0 ||
        strcmp(api_hash, "abcdef0123456789") != 0) {
        (void)remove(api_path);
        return 2;
    }
    if (tg_mtproto_load_api_id_file(api_path, api_id, sizeof(api_id),
                                    0, 0) != 0 ||
        strcmp(api_id, "12345") != 0) {
        (void)remove(api_path);
        return 2;
    }
    tg_mtproto_secure_zero(api_hash, sizeof(api_hash));
    (void)remove(api_path);

    quiet = tmpfile();
    if (quiet == 0) {
        return 2;
    }
    (void)remove(peer_path);
    if (tg_file_write_text(peer_path, peer_text,
                           (unsigned long)strlen(peer_text)) != TG_FILE_OK) {
        fclose(quiet);
        return 2;
    }
    if (tg_mtproto_load_peer_cache_peer(peer_path, "2", &peer_constructor,
                                        &peer_id_hi, &peer_id_lo,
                                        &access_hash_hi, &access_hash_lo,
                                        &has_access_hash, quiet,
                                        "peer-cache-self-test") != 0 ||
        peer_constructor != TG_MTPROTO_PEER_CHAT_CONSTRUCTOR ||
        peer_id_hi != 0UL || peer_id_lo != 3UL || has_access_hash) {
        fclose(quiet);
        (void)remove(peer_path);
        return 2;
    }
    if (tg_mtproto_load_peer_cache_peer(peer_path, "3", &peer_constructor,
                                        &peer_id_hi, &peer_id_lo,
                                        &access_hash_hi, &access_hash_lo,
                                        &has_access_hash, quiet,
                                        "peer-cache-self-test") != 0 ||
        peer_constructor != TG_MTPROTO_PEER_CHANNEL_CONSTRUCTOR ||
        peer_id_hi != 0UL || peer_id_lo != 4UL ||
        access_hash_hi != 0UL || access_hash_lo != 5UL ||
        !has_access_hash) {
        fclose(quiet);
        (void)remove(peer_path);
        return 2;
    }
    fclose(quiet);
    (void)remove(peer_path);

    return 0;
}
