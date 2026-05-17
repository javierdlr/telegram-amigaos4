/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_mtproto_login.h"

#define TG_MTPROTO_CURRENT_LAYER 214UL
#define TG_INVOKE_WITH_LAYER_CONSTRUCTOR 0xda9b0d0dUL
#define TG_INIT_CONNECTION_CONSTRUCTOR 0xc1cd5ea9UL
#define TG_CODE_SETTINGS_CONSTRUCTOR 0xad253d78UL
#define TG_AUTH_SEND_CODE_CONSTRUCTOR 0xa677244fUL
#define TG_AUTH_SIGN_IN_CONSTRUCTOR 0x8d52a951UL
#define TG_HELP_GET_CONFIG_CONSTRUCTOR 0xc4f9186bUL
#define TG_ACCOUNT_GET_PASSWORD_CONSTRUCTOR 0x548a30f5UL
#define TG_MSGS_ACK_CONSTRUCTOR 0x62d6b459UL
#define TG_VECTOR_CONSTRUCTOR 0x1cb5c415UL
#define TG_RPC_RESULT_CONSTRUCTOR 0xf35c6d01UL
#define TG_RPC_ERROR_CONSTRUCTOR 0x2144ca19UL
#define TG_BAD_MSG_NOTIFICATION_CONSTRUCTOR 0xa7eff811UL
#define TG_BAD_SERVER_SALT_CONSTRUCTOR 0xedab447bUL
#define TG_AUTH_SENT_CODE_CONSTRUCTOR 0x5e002502UL
#define TG_AUTH_SENT_CODE_SUCCESS_CONSTRUCTOR 0x2390fe44UL
#define TG_AUTH_SENT_CODE_PAYMENT_REQUIRED_CONSTRUCTOR 0xd7a2fcf9UL
#define TG_AUTH_AUTHORIZATION_CONSTRUCTOR 0x2ea2c0d4UL
#define TG_AUTH_AUTHORIZATION_SIGNUP_REQUIRED_CONSTRUCTOR 0x44747e9aUL
#define TG_CONFIG_CONSTRUCTOR 0xcc1a241eUL
#define TG_ACCOUNT_PASSWORD_CONSTRUCTOR 0x957b50fbUL

static tg_mtproto_tl_status tg_write_string(tg_mtproto_tl_writer *writer,
                                            const char *text)
{
    if (text == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    return tg_mtproto_tl_write_bytes(writer, (const unsigned char *)text,
                                     (unsigned long)strlen(text));
}

static tg_mtproto_tl_status tg_read_string_copy(tg_mtproto_tl_reader *reader,
                                                char *buffer,
                                                unsigned long buffer_size)
{
    const unsigned char *bytes;
    unsigned long length;
    unsigned long copy_length;
    tg_mtproto_tl_status status;

    if (buffer == 0 || buffer_size == 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    buffer[0] = '\0';
    status = tg_mtproto_tl_read_bytes(reader, &bytes, &length);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    copy_length = length;
    if (copy_length >= buffer_size) {
        copy_length = buffer_size - 1UL;
    }
    memcpy(buffer, bytes, (size_t)copy_length);
    buffer[copy_length] = '\0';
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_string(tg_mtproto_tl_reader *reader)
{
    const unsigned char *bytes;
    unsigned long length;

    return tg_mtproto_tl_read_bytes(reader, &bytes, &length);
}

static tg_mtproto_tl_status tg_skip_auth_sent_code_type(
    tg_mtproto_tl_reader *reader,
    unsigned long *type_constructor)
{
    unsigned long constructor;
    unsigned long flags;
    tg_mtproto_tl_status status;

    status = tg_mtproto_tl_read_u32(reader, &constructor);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    if (type_constructor != 0) {
        *type_constructor = constructor;
    }

    switch (constructor) {
    case 0x3dbb5986UL: /* auth.sentCodeTypeApp */
    case 0xc000bba2UL: /* auth.sentCodeTypeSms */
    case 0x5353e5a7UL: /* auth.sentCodeTypeCall */
        return tg_mtproto_tl_read_u32(reader, &flags);
    case 0xab03c6d9UL: /* auth.sentCodeTypeFlashCall */
        return tg_skip_string(reader);
    case 0x82006484UL: /* auth.sentCodeTypeMissedCall */
    case 0xd9565c39UL: /* auth.sentCodeTypeFragmentSms */
        status = tg_skip_string(reader);
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_mtproto_tl_read_u32(reader, &flags);
        }
        return status;
    case 0xf450f59bUL: /* auth.sentCodeTypeEmailCode */
        status = tg_mtproto_tl_read_u32(reader, &flags);
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_skip_string(reader);
        }
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_mtproto_tl_read_u32(reader, &constructor);
        }
        if (status == TG_MTPROTO_TL_OK && (flags & 8UL) != 0UL) {
            status = tg_mtproto_tl_read_u32(reader, &constructor);
        }
        if (status == TG_MTPROTO_TL_OK && (flags & 16UL) != 0UL) {
            status = tg_mtproto_tl_read_u32(reader, &constructor);
        }
        return status;
    case 0xa5491deaUL: /* auth.sentCodeTypeSetUpEmailRequired */
        return tg_mtproto_tl_read_u32(reader, &flags);
    case 0x009fd736UL: /* auth.sentCodeTypeFirebaseSms */
        status = tg_mtproto_tl_read_u32(reader, &flags);
        if (status == TG_MTPROTO_TL_OK && (flags & 1UL) != 0UL) {
            status = tg_skip_string(reader);
        }
        if (status == TG_MTPROTO_TL_OK && (flags & 4UL) != 0UL) {
            status = tg_mtproto_tl_read_u32(reader, &constructor);
        }
        if (status == TG_MTPROTO_TL_OK && (flags & 4UL) != 0UL) {
            status = tg_mtproto_tl_read_u32(reader, &constructor);
        }
        if (status == TG_MTPROTO_TL_OK && (flags & 4UL) != 0UL) {
            status = tg_skip_string(reader);
        }
        if (status == TG_MTPROTO_TL_OK && (flags & 2UL) != 0UL) {
            status = tg_skip_string(reader);
        }
        if (status == TG_MTPROTO_TL_OK && (flags & 2UL) != 0UL) {
            status = tg_mtproto_tl_read_u32(reader, &constructor);
        }
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_mtproto_tl_read_u32(reader, &constructor);
        }
        return status;
    case 0xa416ac81UL: /* auth.sentCodeTypeSmsWord */
    case 0xb37794afUL: /* auth.sentCodeTypeSmsPhrase */
        status = tg_mtproto_tl_read_u32(reader, &flags);
        if (status == TG_MTPROTO_TL_OK && (flags & 1UL) != 0UL) {
            status = tg_skip_string(reader);
        }
        return status;
    default:
        return TG_MTPROTO_TL_INVALID_DATA;
    }
}

tg_mtproto_tl_status tg_mtproto_build_invoke_with_layer(
    tg_mtproto_tl_writer *writer,
    unsigned long layer,
    const unsigned char *query,
    unsigned long query_length)
{
    tg_mtproto_tl_status status;

    if (query == 0 && query_length > 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, TG_INVOKE_WITH_LAYER_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, layer);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, query, query_length);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_init_connection(
    tg_mtproto_tl_writer *writer,
    unsigned long api_id,
    const char *device_model,
    const char *system_version,
    const char *app_version,
    const char *lang_code,
    const unsigned char *query,
    unsigned long query_length)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || device_model == 0 || system_version == 0 ||
        app_version == 0 || lang_code == 0 ||
        (query == 0 && query_length > 0UL) ||
        device_model[0] == '\0' || system_version[0] == '\0' ||
        app_version[0] == '\0' || lang_code[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, TG_INIT_CONNECTION_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, api_id);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, device_model);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, system_version);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, app_version);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, lang_code);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, "");
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, lang_code);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, query, query_length);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_auth_send_code(
    tg_mtproto_tl_writer *writer,
    const char *phone_number,
    unsigned long api_id,
    const char *api_hash)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || phone_number == 0 || api_hash == 0 ||
        phone_number[0] == '\0' || api_hash[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, TG_AUTH_SEND_CODE_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, phone_number);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, api_id);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, api_hash);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, TG_CODE_SETTINGS_CONSTRUCTOR);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_auth_sign_in(
    tg_mtproto_tl_writer *writer,
    const char *phone_number,
    const char *phone_code_hash,
    const char *phone_code)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || phone_number == 0 || phone_code_hash == 0 ||
        phone_code == 0 || phone_number[0] == '\0' ||
        phone_code_hash[0] == '\0' || phone_code[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, TG_AUTH_SIGN_IN_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 1UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, phone_number);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, phone_code_hash);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, phone_code);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_help_get_config(
    tg_mtproto_tl_writer *writer)
{
    return tg_mtproto_tl_write_u32(writer, TG_HELP_GET_CONFIG_CONSTRUCTOR);
}

tg_mtproto_tl_status tg_mtproto_build_account_get_password(
    tg_mtproto_tl_writer *writer)
{
    return tg_mtproto_tl_write_u32(writer, TG_ACCOUNT_GET_PASSWORD_CONSTRUCTOR);
}

tg_mtproto_tl_status tg_mtproto_build_msgs_ack(
    tg_mtproto_tl_writer *writer,
    const unsigned long *msg_id_hi,
    const unsigned long *msg_id_lo,
    unsigned long msg_id_count)
{
    unsigned long i;
    tg_mtproto_tl_status status;

    if (writer == 0 || msg_id_count == 0UL || msg_id_hi == 0 ||
        msg_id_lo == 0 || msg_id_count > 8192UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, TG_MSGS_ACK_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, TG_VECTOR_CONSTRUCTOR);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, msg_id_count);
    }
    for (i = 0UL; status == TG_MTPROTO_TL_OK && i < msg_id_count; ++i) {
        status = tg_mtproto_tl_write_u64(writer, msg_id_hi[i], msg_id_lo[i]);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_parse_rpc_result(
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_rpc_result *out)
{
    tg_mtproto_tl_reader reader;
    unsigned long constructor;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (tg_mtproto_tl_read_u32(&reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_RPC_RESULT_CONSTRUCTOR ||
        tg_mtproto_tl_read_u64(&reader, &out->request_msg_id_hi,
                               &out->request_msg_id_lo) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->result_constructor) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->result_body = body + reader.offset;
    out->result_body_length = body_length - reader.offset;
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_rpc_error(
    const unsigned char *body,
    unsigned long body_length,
    long *error_code,
    char *error_message,
    unsigned long error_message_size)
{
    tg_mtproto_tl_reader reader;
    const unsigned char *message;
    unsigned long constructor;
    unsigned long code;
    unsigned long message_length;
    unsigned long copy_length;

    if (body == 0 || error_code == 0 || error_message == 0 ||
        error_message_size == 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    error_message[0] = '\0';
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (tg_mtproto_tl_read_u32(&reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_RPC_ERROR_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(&reader, &code) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_bytes(&reader, &message, &message_length) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    *error_code = (long)code;
    copy_length = message_length;
    if (copy_length >= error_message_size) {
        copy_length = error_message_size - 1UL;
    }
    memcpy(error_message, message, (size_t)copy_length);
    error_message[copy_length] = '\0';
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_bad_msg_notification(
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_bad_msg_notification *out)
{
    tg_mtproto_tl_reader reader;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (tg_mtproto_tl_read_u32(&reader, &out->constructor) !=
            TG_MTPROTO_TL_OK ||
        (out->constructor != TG_BAD_MSG_NOTIFICATION_CONSTRUCTOR &&
         out->constructor != TG_BAD_SERVER_SALT_CONSTRUCTOR) ||
        tg_mtproto_tl_read_u64(&reader, &out->bad_msg_id_hi,
                               &out->bad_msg_id_lo) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->bad_msg_seqno) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->error_code) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (out->constructor == TG_BAD_SERVER_SALT_CONSTRUCTOR) {
        if (tg_mtproto_tl_read_u64(&reader, &out->new_server_salt_hi,
                                   &out->new_server_salt_lo) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        out->has_new_server_salt = 1;
    }
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_auth_sent_code(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_sent_code *out)
{
    tg_mtproto_tl_reader reader;
    unsigned long flags;
    unsigned long unused;
    tg_mtproto_tl_status status;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    out->constructor = constructor;
    tg_mtproto_tl_reader_init(&reader, body, body_length);

    if (constructor == TG_AUTH_SENT_CODE_PAYMENT_REQUIRED_CONSTRUCTOR) {
        status = tg_skip_string(&reader);
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_read_string_copy(&reader, out->phone_code_hash,
                                         sizeof(out->phone_code_hash));
        }
        return status;
    }
    if (constructor == TG_AUTH_SENT_CODE_SUCCESS_CONSTRUCTOR) {
        return TG_MTPROTO_TL_OK;
    }
    if (constructor != TG_AUTH_SENT_CODE_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }

    status = tg_mtproto_tl_read_u32(&reader, &flags);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_skip_auth_sent_code_type(&reader, &out->type_constructor);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_read_string_copy(&reader, out->phone_code_hash,
                                     sizeof(out->phone_code_hash));
    }
    if (status == TG_MTPROTO_TL_OK && (flags & 2UL) != 0UL) {
        status = tg_mtproto_tl_read_u32(&reader, &unused);
    }
    if (status == TG_MTPROTO_TL_OK && (flags & 4UL) != 0UL) {
        status = tg_mtproto_tl_read_u32(&reader, &out->timeout);
        out->has_timeout = 1;
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_parse_config_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_config_summary *out)
{
    tg_mtproto_tl_reader reader;
    unsigned long flags;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    if (constructor != TG_CONFIG_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (tg_mtproto_tl_read_u32(&reader, &flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->date) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->expires) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->test_mode_constructor) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->this_dc) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_account_password_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_password_summary *out)
{
    tg_mtproto_tl_reader reader;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    if (constructor != TG_ACCOUNT_PASSWORD_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (tg_mtproto_tl_read_u32(&reader, &out->flags) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->has_recovery = (out->flags & 1UL) != 0UL;
    out->has_secure_values = (out->flags & 2UL) != 0UL;
    out->has_password = (out->flags & 4UL) != 0UL;
    return TG_MTPROTO_TL_OK;
}

int tg_mtproto_is_auth_authorization_constructor(unsigned long constructor)
{
    return constructor == TG_AUTH_AUTHORIZATION_CONSTRUCTOR ||
           constructor == TG_AUTH_AUTHORIZATION_SIGNUP_REQUIRED_CONSTRUCTOR;
}

int tg_mtproto_login_self_test(void)
{
    static const unsigned char expected_send_code[] = {
        0x4fU, 0x24U, 0x77U, 0xa6U,
        0x0bU, '+', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
        0x2aU, 0x00U, 0x00U, 0x00U,
        0x08U, 'a', 'p', 'i', 'h', 'a', 's', 'h', '1',
        0x00U, 0x00U, 0x00U,
        0x78U, 0x3dU, 0x25U, 0xadU,
        0x00U, 0x00U, 0x00U, 0x00U
    };
    static const unsigned char expected_sign_in[] = {
        0x51U, 0xa9U, 0x52U, 0x8dU,
        0x01U, 0x00U, 0x00U, 0x00U,
        0x02U, '+', '1', 0x00U,
        0x04U, 'h', 'a', 's', 'h', 0x00U, 0x00U, 0x00U,
        0x05U, '1', '2', '3', '4', '5', 0x00U, 0x00U
    };
    unsigned char query[128];
    unsigned char initialized[192];
    unsigned char wrapped[160];
    unsigned char rpc[64];
    char error_text[32];
    long error_code;
    tg_mtproto_bad_msg_notification bad_msg;
    tg_mtproto_config_summary config;
    tg_mtproto_password_summary password;
    tg_mtproto_rpc_result result;
    tg_mtproto_sent_code sent_code;
    tg_mtproto_tl_writer writer;

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_send_code(&writer, "+1234567890", 42UL,
                                        "apihash1") != TG_MTPROTO_TL_OK ||
        writer.length != sizeof(expected_send_code) ||
        memcmp(query, expected_send_code, sizeof(expected_send_code)) != 0) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, wrapped, sizeof(wrapped));
    if (tg_mtproto_build_invoke_with_layer(&writer, TG_MTPROTO_CURRENT_LAYER,
                                           query,
                                           sizeof(expected_send_code)) !=
            TG_MTPROTO_TL_OK ||
        writer.length != sizeof(expected_send_code) + 8UL ||
        wrapped[0] != 0x0dU || wrapped[1] != 0x0dU ||
        wrapped[2] != 0x9bU || wrapped[3] != 0xdaU ||
        wrapped[4] != 214U) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, initialized, sizeof(initialized));
    if (tg_mtproto_build_init_connection(&writer, 42UL, "Amiga",
                                         "portable", "0.1", "en", query,
                                         sizeof(expected_send_code)) !=
            TG_MTPROTO_TL_OK ||
        initialized[0] != 0xa9U || initialized[1] != 0x5eU ||
        initialized[2] != 0xcdU || initialized[3] != 0xc1U) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_sign_in(&writer, "+1", "hash", "12345") !=
            TG_MTPROTO_TL_OK ||
        writer.length != sizeof(expected_sign_in) ||
        memcmp(query, expected_sign_in, sizeof(expected_sign_in)) != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_RPC_RESULT_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x11223344UL, 0x55667788UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0x5e002502UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_rpc_result(rpc, writer.length, &result) !=
            TG_MTPROTO_TL_OK ||
        result.request_msg_id_hi != 0x11223344UL ||
        result.request_msg_id_lo != 0x55667788UL ||
        result.result_constructor != 0x5e002502UL ||
        result.result_body_length != 4UL) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_RPC_ERROR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 400UL) != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "PHONE_NUMBER_INVALID") !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_rpc_error(rpc, writer.length, &error_code,
                                   error_text, sizeof(error_text)) !=
            TG_MTPROTO_TL_OK ||
        error_code != 400L ||
        strcmp(error_text, "PHONE_NUMBER_INVALID") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_BAD_SERVER_SALT_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x01020304UL, 0x05060708UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 48UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x99aabbccUL, 0xddeeff00UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_bad_msg_notification(rpc, writer.length, &bad_msg) !=
            TG_MTPROTO_TL_OK ||
        bad_msg.error_code != 48UL ||
        bad_msg.new_server_salt_hi != 0x99aabbccUL ||
        bad_msg.new_server_salt_lo != 0xddeeff00UL ||
        !bad_msg.has_new_server_salt) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0x3dbb5986UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 5UL) != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "hash") != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_auth_sent_code(TG_AUTH_SENT_CODE_CONSTRUCTOR, rpc,
                                        writer.length, &sent_code) !=
            TG_MTPROTO_TL_OK ||
        sent_code.type_constructor != 0x3dbb5986UL ||
        strcmp(sent_code.phone_code_hash, "hash") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_help_get_config(&writer) != TG_MTPROTO_TL_OK ||
        writer.length != 4UL ||
        query[0] != 0x6bU || query[1] != 0x18U ||
        query[2] != 0xf9U || query[3] != 0xc4U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_account_get_password(&writer) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 4UL ||
        query[0] != 0xf5U || query[1] != 0x30U ||
        query[2] != 0x8aU || query[3] != 0x54U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_msgs_ack(&writer, &bad_msg.bad_msg_id_hi,
                                  &bad_msg.bad_msg_id_lo, 1UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 20UL ||
        query[0] != 0x59U || query[1] != 0xb4U ||
        query[2] != 0xd6U || query[3] != 0x62U ||
        query[4] != 0x15U || query[5] != 0xc4U ||
        query[6] != 0xb5U || query[7] != 0x1cU) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 100UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 200UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0xbc799737UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_config_summary(TG_CONFIG_CONSTRUCTOR, rpc,
                                        writer.length, &config) !=
            TG_MTPROTO_TL_OK ||
        config.date != 100UL || config.expires != 200UL ||
        config.this_dc != 2UL) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, 7UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_account_password_summary(
            TG_ACCOUNT_PASSWORD_CONSTRUCTOR, rpc, writer.length,
            &password) != TG_MTPROTO_TL_OK ||
        !password.has_recovery || !password.has_secure_values ||
        !password.has_password) {
        return 2;
    }

    return 0;
}
