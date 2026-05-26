/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_mtproto_login.h"

#define TG_MTPROTO_CURRENT_LAYER 214UL
#define TG_INVOKE_WITH_LAYER_CONSTRUCTOR 0xda9b0d0dUL
#define TG_INVOKE_WITHOUT_UPDATES_CONSTRUCTOR 0xbf9459b7UL
#define TG_INIT_CONNECTION_CONSTRUCTOR 0xc1cd5ea9UL
#define TG_CODE_SETTINGS_CONSTRUCTOR 0xad253d78UL
#define TG_AUTH_SEND_CODE_CONSTRUCTOR 0xa677244fUL
#define TG_AUTH_SIGN_IN_CONSTRUCTOR 0x8d52a951UL
#define TG_AUTH_SIGN_UP_CONSTRUCTOR 0xaac7b717UL
#define TG_AUTH_CHECK_PASSWORD_CONSTRUCTOR 0xd18b4d16UL
#define TG_HELP_GET_CONFIG_CONSTRUCTOR 0xc4f9186bUL
#define TG_ACCOUNT_GET_PASSWORD_CONSTRUCTOR 0x548a30f5UL
#define TG_INPUT_CHECK_PASSWORD_EMPTY_CONSTRUCTOR 0x9880f658UL
#define TG_INPUT_CHECK_PASSWORD_SRP_CONSTRUCTOR 0xd27ff082UL
#define TG_USERS_GET_USERS_CONSTRUCTOR 0x0d91a548UL
#define TG_INPUT_USER_SELF_CONSTRUCTOR 0xf7c1b13fUL
#define TG_MESSAGES_GET_DIALOGS_CONSTRUCTOR 0xa0f4cb4fUL
#define TG_MESSAGES_GET_HISTORY_CONSTRUCTOR 0x4423e6c5UL
#define TG_MESSAGES_SEND_MESSAGE_CONSTRUCTOR 0xfe05dc9aUL
#define TG_CONTACTS_RESOLVE_USERNAME_CONSTRUCTOR 0xf93ccba3UL
#define TG_CONTACTS_RESOLVE_USERNAME_FLAGS_CONSTRUCTOR 0x725afbbcUL
#define TG_CONTACTS_SEARCH_CONSTRUCTOR 0x11f812d8UL
#define TG_INPUT_PEER_EMPTY_CONSTRUCTOR 0x7f3b18eaUL
#define TG_INPUT_PEER_SELF_CONSTRUCTOR 0x7da07ec9UL
#define TG_INPUT_PEER_USER_CONSTRUCTOR 0xdde8a54cUL
#define TG_INPUT_PEER_CHAT_CONSTRUCTOR 0x35a95cb9UL
#define TG_INPUT_PEER_CHANNEL_CONSTRUCTOR 0x27bcbbfcUL
#define TG_PEER_USER_CONSTRUCTOR 0x59511722UL
#define TG_PEER_CHAT_CONSTRUCTOR 0x36c6019aUL
#define TG_PEER_CHANNEL_CONSTRUCTOR 0xa2a5371eUL
#define TG_DIALOG_CONSTRUCTOR 0xd58a08c6UL
#define TG_PEER_NOTIFY_SETTINGS_CONSTRUCTOR 0x99622c0cUL
#define TG_NOTIFICATION_SOUND_DEFAULT_CONSTRUCTOR 0x97e8bebeUL
#define TG_NOTIFICATION_SOUND_NONE_CONSTRUCTOR 0x6f0c34dfUL
#define TG_NOTIFICATION_SOUND_LOCAL_CONSTRUCTOR 0x830b9ae4UL
#define TG_NOTIFICATION_SOUND_RINGTONE_CONSTRUCTOR 0xff6c8049UL
#define TG_DRAFT_MESSAGE_EMPTY_CONSTRUCTOR 0x1b0c841aUL
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
#define TG_PASSWORD_KDF_ALGO_SRP_CONSTRUCTOR 0x3a912d4aUL
#define TG_USER_CONSTRUCTOR 0x020b1422UL
#define TG_CHAT_EMPTY_CONSTRUCTOR 0x29562865UL
#define TG_CHAT_CONSTRUCTOR 0x41cbf256UL
#define TG_CHAT_FORBIDDEN_CONSTRUCTOR 0x6592a1a7UL
#define TG_CHANNEL_CONSTRUCTOR 0xfe685355UL
#define TG_CHANNEL_FORBIDDEN_CONSTRUCTOR 0x17d493d5UL
#define TG_MESSAGE_EMPTY_CONSTRUCTOR 0x90a6ca84UL
#define TG_MESSAGE_CONSTRUCTOR 0x9815cec8UL
#define TG_MESSAGE_SERVICE_CONSTRUCTOR 0x7a800e0aUL
#define TG_MESSAGE_FWD_HEADER_CONSTRUCTOR 0x4e4df4bbUL
#define TG_MESSAGE_REPLY_HEADER_CONSTRUCTOR 0x6917560bUL
#define TG_MESSAGE_REPLIES_CONSTRUCTOR 0x83d60fc2UL
#define TG_MESSAGE_REACTIONS_CONSTRUCTOR 0x0a339f0bUL
#define TG_REACTION_COUNT_CONSTRUCTOR 0xa3d1cb80UL
#define TG_MESSAGES_DIALOGS_CONSTRUCTOR 0x15ba6c40UL
#define TG_MESSAGES_DIALOGS_SLICE_CONSTRUCTOR 0x71e094f3UL
#define TG_MESSAGES_DIALOGS_NOT_MODIFIED_CONSTRUCTOR 0xf0e3e596UL
#define TG_MESSAGES_MESSAGES_CONSTRUCTOR 0x8c718e87UL
#define TG_MESSAGES_MESSAGES_SLICE_CONSTRUCTOR 0x762b263dUL
#define TG_MESSAGES_CHANNEL_MESSAGES_CONSTRUCTOR 0xc776ba4eUL
#define TG_MESSAGES_MESSAGES_NOT_MODIFIED_CONSTRUCTOR 0x74535f21UL
#define TG_CONTACTS_RESOLVED_PEER_CONSTRUCTOR 0x7f077ad9UL
#define TG_CONTACTS_FOUND_CONSTRUCTOR 0xb3134d9dUL
#define TG_UPDATE_SHORT_SENT_MESSAGE_CONSTRUCTOR 0x9015e101UL

static unsigned long tg_read_u32_le(const unsigned char *p)
{
    return ((unsigned long)p[0]) |
           (((unsigned long)p[1]) << 8) |
           (((unsigned long)p[2]) << 16) |
           (((unsigned long)p[3]) << 24);
}

static tg_mtproto_tl_status tg_write_input_peer(
    tg_mtproto_tl_writer *writer,
    unsigned long peer_constructor,
    unsigned long peer_id_hi,
    unsigned long peer_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo,
    int has_access_hash);

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

static tg_mtproto_tl_status tg_read_bytes_copy(
    tg_mtproto_tl_reader *reader,
    unsigned char *buffer,
    unsigned long buffer_size,
    unsigned long *decoded_length)
{
    const unsigned char *bytes;
    unsigned long length;
    tg_mtproto_tl_status status;

    if (buffer == 0 || decoded_length == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_read_bytes(reader, &bytes, &length);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    if (length > buffer_size) {
        return TG_MTPROTO_TL_BUFFER_TOO_SMALL;
    }
    if (length > 0UL) {
        memcpy(buffer, bytes, (size_t)length);
    }
    *decoded_length = length;
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
    unsigned long *type_constructor,
    unsigned long *type_length,
    int *has_type_length)
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
    if (type_length != 0) {
        *type_length = 0UL;
    }
    if (has_type_length != 0) {
        *has_type_length = 0;
    }

    switch (constructor) {
    case 0x3dbb5986UL: /* auth.sentCodeTypeApp */
    case 0xc000bba2UL: /* auth.sentCodeTypeSms */
    case 0x5353e5a7UL: /* auth.sentCodeTypeCall */
        status = tg_mtproto_tl_read_u32(reader, &flags);
        if (status == TG_MTPROTO_TL_OK) {
            if (type_length != 0) {
                *type_length = flags;
            }
            if (has_type_length != 0) {
                *has_type_length = 1;
            }
        }
        return status;
    case 0xab03c6d9UL: /* auth.sentCodeTypeFlashCall */
        return tg_skip_string(reader);
    case 0x82006484UL: /* auth.sentCodeTypeMissedCall */
    case 0xd9565c39UL: /* auth.sentCodeTypeFragmentSms */
        status = tg_skip_string(reader);
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_mtproto_tl_read_u32(reader, &flags);
        }
        if (status == TG_MTPROTO_TL_OK) {
            if (type_length != 0) {
                *type_length = flags;
            }
            if (has_type_length != 0) {
                *has_type_length = 1;
            }
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
        if (status == TG_MTPROTO_TL_OK) {
            if (type_length != 0) {
                *type_length = constructor;
            }
            if (has_type_length != 0) {
                *has_type_length = 1;
            }
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
        if (status == TG_MTPROTO_TL_OK) {
            if (type_length != 0) {
                *type_length = constructor;
            }
            if (has_type_length != 0) {
                *has_type_length = 1;
            }
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

tg_mtproto_tl_status tg_mtproto_build_invoke_without_updates(
    tg_mtproto_tl_writer *writer,
    const unsigned char *query,
    unsigned long query_length)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || query == 0 || query_length == 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(
        writer, TG_INVOKE_WITHOUT_UPDATES_CONSTRUCTOR);
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

tg_mtproto_tl_status tg_mtproto_build_auth_sign_up(
    tg_mtproto_tl_writer *writer,
    const char *phone_number,
    const char *phone_code_hash,
    const char *first_name,
    const char *last_name)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || phone_number == 0 || phone_code_hash == 0 ||
        first_name == 0 || last_name == 0 || phone_number[0] == '\0' ||
        phone_code_hash[0] == '\0' || first_name[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, TG_AUTH_SIGN_UP_CONSTRUCTOR);
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
        status = tg_write_string(writer, first_name);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, last_name);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_input_check_password_empty(
    tg_mtproto_tl_writer *writer)
{
    return tg_mtproto_tl_write_u32(
        writer, TG_INPUT_CHECK_PASSWORD_EMPTY_CONSTRUCTOR);
}

tg_mtproto_tl_status tg_mtproto_build_input_check_password_srp(
    tg_mtproto_tl_writer *writer,
    unsigned long srp_id_hi,
    unsigned long srp_id_lo,
    const unsigned char *a,
    unsigned long a_length,
    const unsigned char m1[TG_MTPROTO_SHA256_LENGTH])
{
    tg_mtproto_tl_status status;

    if (writer == 0 || a == 0 || m1 == 0 || a_length == 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_INPUT_CHECK_PASSWORD_SRP_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, srp_id_hi, srp_id_lo);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, a, a_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, m1,
                                           TG_MTPROTO_SHA256_LENGTH);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_auth_check_password_empty(
    tg_mtproto_tl_writer *writer)
{
    tg_mtproto_tl_status status;

    if (writer == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_AUTH_CHECK_PASSWORD_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_build_input_check_password_empty(writer);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_auth_check_password_srp(
    tg_mtproto_tl_writer *writer,
    unsigned long srp_id_hi,
    unsigned long srp_id_lo,
    const unsigned char *a,
    unsigned long a_length,
    const unsigned char m1[TG_MTPROTO_SHA256_LENGTH])
{
    tg_mtproto_tl_status status;

    if (writer == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_AUTH_CHECK_PASSWORD_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_build_input_check_password_srp(
            writer, srp_id_hi, srp_id_lo, a, a_length, m1);
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

tg_mtproto_tl_status tg_mtproto_build_users_get_self(
    tg_mtproto_tl_writer *writer)
{
    tg_mtproto_tl_status status;

    status = tg_mtproto_tl_write_u32(writer, TG_USERS_GET_USERS_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, TG_VECTOR_CONSTRUCTOR);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 1UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, TG_INPUT_USER_SELF_CONSTRUCTOR);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_messages_get_dialogs(
    tg_mtproto_tl_writer *writer,
    unsigned long limit)
{
    return tg_mtproto_build_messages_get_dialogs_page(
        writer, limit, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0);
}

tg_mtproto_tl_status tg_mtproto_build_messages_get_dialogs_page(
    tg_mtproto_tl_writer *writer,
    unsigned long limit,
    unsigned long offset_id,
    unsigned long offset_peer_constructor,
    unsigned long offset_peer_id_hi,
    unsigned long offset_peer_id_lo,
    unsigned long offset_access_hash_hi,
    unsigned long offset_access_hash_lo,
    int offset_has_access_hash)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || limit == 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_MESSAGES_GET_DIALOGS_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, offset_id);
    }
    if (status == TG_MTPROTO_TL_OK) {
        if (offset_id == 0UL || offset_peer_constructor == 0UL) {
            status = tg_mtproto_tl_write_u32(writer,
                                             TG_INPUT_PEER_EMPTY_CONSTRUCTOR);
        } else {
            status = tg_write_input_peer(writer, offset_peer_constructor,
                                         offset_peer_id_hi, offset_peer_id_lo,
                                         offset_access_hash_hi,
                                         offset_access_hash_lo,
                                         offset_has_access_hash);
        }
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, limit);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, 0UL, 0UL);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_messages_get_history_self(
    tg_mtproto_tl_writer *writer,
    unsigned long limit)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || limit == 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_MESSAGES_GET_HISTORY_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, TG_INPUT_PEER_SELF_CONSTRUCTOR);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, limit);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, 0UL, 0UL);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_contacts_resolve_username(
    tg_mtproto_tl_writer *writer,
    const char *username)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || username == 0 || username[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(
        writer, TG_CONTACTS_RESOLVE_USERNAME_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, username);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_contacts_resolve_username_flags(
    tg_mtproto_tl_writer *writer,
    const char *username)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || username == 0 || username[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(
        writer, TG_CONTACTS_RESOLVE_USERNAME_FLAGS_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, username);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_contacts_search(
    tg_mtproto_tl_writer *writer,
    const char *query,
    unsigned long limit)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || query == 0 || query[0] == '\0' || limit == 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, TG_CONTACTS_SEARCH_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, query);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, limit);
    }
    return status;
}

static tg_mtproto_tl_status tg_write_input_peer_user(
    tg_mtproto_tl_writer *writer,
    unsigned long user_id_hi,
    unsigned long user_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo)
{
    tg_mtproto_tl_status status;

    status = tg_mtproto_tl_write_u32(writer, TG_INPUT_PEER_USER_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, user_id_hi, user_id_lo);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, access_hash_hi,
                                         access_hash_lo);
    }
    return status;
}

static tg_mtproto_tl_status tg_write_input_peer(
    tg_mtproto_tl_writer *writer,
    unsigned long peer_constructor,
    unsigned long peer_id_hi,
    unsigned long peer_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo,
    int has_access_hash)
{
    tg_mtproto_tl_status status;

    if (writer == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    switch (peer_constructor) {
    case TG_PEER_USER_CONSTRUCTOR:
        if (!has_access_hash) {
            return TG_MTPROTO_TL_INVALID_ARGUMENT;
        }
        return tg_write_input_peer_user(writer, peer_id_hi, peer_id_lo,
                                        access_hash_hi, access_hash_lo);
    case TG_PEER_CHAT_CONSTRUCTOR:
        status = tg_mtproto_tl_write_u32(writer, TG_INPUT_PEER_CHAT_CONSTRUCTOR);
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_mtproto_tl_write_u64(writer, peer_id_hi, peer_id_lo);
        }
        return status;
    case TG_PEER_CHANNEL_CONSTRUCTOR:
        if (!has_access_hash) {
            return TG_MTPROTO_TL_INVALID_ARGUMENT;
        }
        status = tg_mtproto_tl_write_u32(writer,
                                         TG_INPUT_PEER_CHANNEL_CONSTRUCTOR);
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_mtproto_tl_write_u64(writer, peer_id_hi, peer_id_lo);
        }
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_mtproto_tl_write_u64(writer, access_hash_hi,
                                             access_hash_lo);
        }
        return status;
    default:
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
}

tg_mtproto_tl_status tg_mtproto_build_messages_get_history_peer(
    tg_mtproto_tl_writer *writer,
    unsigned long peer_constructor,
    unsigned long peer_id_hi,
    unsigned long peer_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo,
    int has_access_hash,
    unsigned long limit)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || limit == 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_MESSAGES_GET_HISTORY_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_input_peer(writer, peer_constructor, peer_id_hi,
                                     peer_id_lo, access_hash_hi,
                                     access_hash_lo, has_access_hash);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, limit);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, 0UL, 0UL);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_messages_get_history_user(
    tg_mtproto_tl_writer *writer,
    unsigned long user_id_hi,
    unsigned long user_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo,
    unsigned long limit)
{
    return tg_mtproto_build_messages_get_history_peer(
        writer, TG_PEER_USER_CONSTRUCTOR, user_id_hi, user_id_lo,
        access_hash_hi, access_hash_lo, 1, limit);
}

tg_mtproto_tl_status tg_mtproto_build_messages_send_self(
    tg_mtproto_tl_writer *writer,
    const char *message,
    unsigned long random_id_hi,
    unsigned long random_id_lo)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || message == 0 || message[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_MESSAGES_SEND_MESSAGE_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, TG_INPUT_PEER_SELF_CONSTRUCTOR);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, message);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, random_id_hi, random_id_lo);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_messages_send_user(
    tg_mtproto_tl_writer *writer,
    unsigned long user_id_hi,
    unsigned long user_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo,
    const char *message,
    unsigned long random_id_hi,
    unsigned long random_id_lo)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || message == 0 || message[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_MESSAGES_SEND_MESSAGE_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_input_peer_user(writer, user_id_hi, user_id_lo,
                                          access_hash_hi, access_hash_lo);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, message);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, random_id_hi, random_id_lo);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_messages_send_peer(
    tg_mtproto_tl_writer *writer,
    unsigned long peer_constructor,
    unsigned long peer_id_hi,
    unsigned long peer_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo,
    int has_access_hash,
    const char *message,
    unsigned long random_id_hi,
    unsigned long random_id_lo)
{
    tg_mtproto_tl_status status;

    if (writer == 0 || message == 0 || message[0] == '\0') {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_MESSAGES_SEND_MESSAGE_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_input_peer(writer, peer_constructor, peer_id_hi,
                                     peer_id_lo, access_hash_hi,
                                     access_hash_lo, has_access_hash);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_write_string(writer, message);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, random_id_hi, random_id_lo);
    }
    return status;
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
        status = tg_skip_auth_sent_code_type(&reader, &out->type_constructor,
                                             &out->type_length,
                                             &out->has_type_length);
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
    tg_mtproto_tl_status status;

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
    if (!out->has_password) {
        return TG_MTPROTO_TL_OK;
    }
    if (tg_mtproto_tl_read_u32(&reader, &out->current_algo_constructor) !=
        TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->has_current_algo = 1;
    if (out->current_algo_constructor != TG_PASSWORD_KDF_ALGO_SRP_CONSTRUCTOR) {
        return TG_MTPROTO_TL_OK;
    }
    status = tg_read_bytes_copy(&reader, out->current_salt1,
                                sizeof(out->current_salt1),
                                &out->current_salt1_length);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_read_bytes_copy(&reader, out->current_salt2,
                                    sizeof(out->current_salt2),
                                    &out->current_salt2_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_read_u32(&reader, &out->current_g);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_read_bytes_copy(&reader, out->current_p,
                                    sizeof(out->current_p),
                                    &out->current_p_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_read_bytes_copy(&reader, out->srp_b, sizeof(out->srp_b),
                                    &out->srp_b_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_read_u64(&reader, &out->srp_id_hi,
                                        &out->srp_id_lo);
    }
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_user_vector_first(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_user_summary *out)
{
    tg_mtproto_tl_reader reader;
    unsigned long count;
    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    if (constructor != TG_VECTOR_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (tg_mtproto_tl_read_u32(&reader, &count) != TG_MTPROTO_TL_OK ||
        count == 0UL ||
        tg_mtproto_tl_read_u32(&reader, &out->constructor) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (out->constructor != TG_USER_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(&reader, &out->flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->flags2) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u64(&reader, &out->id_hi, &out->id_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->is_self = (out->flags & (1UL << 10)) != 0UL;
    out->is_bot = (out->flags & (1UL << 14)) != 0UL;
    if ((out->flags & 1UL) != 0UL &&
        tg_mtproto_tl_read_u64(&reader, &out->access_hash_hi,
                               &out->access_hash_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 1UL) != 0UL) {
        out->has_access_hash = 1;
    }
    if ((out->flags & 2UL) != 0UL &&
        tg_read_string_copy(&reader, out->first_name,
                            sizeof(out->first_name)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 4UL) != 0UL &&
        tg_read_string_copy(&reader, out->last_name,
                            sizeof(out->last_name)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 8UL) != 0UL &&
        tg_read_string_copy(&reader, out->username,
                            sizeof(out->username)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 16UL) != 0UL &&
        tg_read_string_copy(&reader, out->phone,
                            sizeof(out->phone)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_read_vector_count(tg_mtproto_tl_reader *reader,
                                                 unsigned long *count)
{
    unsigned long vector_constructor;

    if (tg_mtproto_tl_read_u32(reader, &vector_constructor) !=
            TG_MTPROTO_TL_OK ||
        vector_constructor != TG_VECTOR_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(reader, count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_bool(tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor != 0xbc799737UL && constructor != 0x997275b5UL) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_notification_sound(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long scratch_hi;
    unsigned long scratch_lo;
    tg_mtproto_tl_status status;

    status = tg_mtproto_tl_read_u32(reader, &constructor);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    switch (constructor) {
    case TG_NOTIFICATION_SOUND_DEFAULT_CONSTRUCTOR:
    case TG_NOTIFICATION_SOUND_NONE_CONSTRUCTOR:
        return TG_MTPROTO_TL_OK;
    case TG_NOTIFICATION_SOUND_LOCAL_CONSTRUCTOR:
        status = tg_skip_string(reader);
        if (status == TG_MTPROTO_TL_OK) {
            status = tg_skip_string(reader);
        }
        return status;
    case TG_NOTIFICATION_SOUND_RINGTONE_CONSTRUCTOR:
        return tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo);
    default:
        return TG_MTPROTO_TL_INVALID_DATA;
    }
}

static tg_mtproto_tl_status tg_skip_peer_notify_settings(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long scratch;
    unsigned long bit;
    tg_mtproto_tl_status status;

    status = tg_mtproto_tl_read_u32(reader, &constructor);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    if (constructor != TG_PEER_NOTIFY_SETTINGS_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    status = tg_mtproto_tl_read_u32(reader, &flags);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    if ((flags & 1UL) != 0UL && tg_skip_bool(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 2UL) != 0UL && tg_skip_bool(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 4UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    for (bit = 8UL; bit <= 32UL; bit <<= 1) {
        if ((flags & bit) != 0UL &&
            tg_skip_notification_sound(reader) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    }
    if ((flags & 64UL) != 0UL && tg_skip_bool(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 128UL) != 0UL && tg_skip_bool(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    for (bit = 256UL; bit <= 1024UL; bit <<= 1) {
        if ((flags & bit) != 0UL &&
            tg_skip_notification_sound(reader) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_draft_message(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor == TG_DRAFT_MESSAGE_EMPTY_CONSTRUCTOR) {
        return TG_MTPROTO_TL_OK;
    }
    return TG_MTPROTO_TL_INVALID_DATA;
}

static tg_mtproto_tl_status tg_read_peer_ref(tg_mtproto_tl_reader *reader,
                                             tg_mtproto_dialog_peer *peer)
{
    if (peer == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    if (tg_mtproto_tl_read_u32(reader, &peer->peer_constructor) !=
        TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    switch (peer->peer_constructor) {
    case TG_PEER_USER_CONSTRUCTOR:
    case TG_PEER_CHAT_CONSTRUCTOR:
    case TG_PEER_CHANNEL_CONSTRUCTOR:
        return tg_mtproto_tl_read_u64(reader, &peer->id_hi, &peer->id_lo);
    default:
        return TG_MTPROTO_TL_INVALID_DATA;
    }
}

static tg_mtproto_tl_status tg_read_dialog_peer(
    tg_mtproto_tl_reader *reader,
    tg_mtproto_dialog_peer *peer)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long scratch;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_DIALOG_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
        tg_read_peer_ref(reader, peer) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &peer->top_message) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &peer->unread_count) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK ||
        tg_skip_peer_notify_settings(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 1UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 2UL) != 0UL &&
        tg_skip_draft_message(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 16UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 32UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

const char *tg_mtproto_peer_constructor_name(unsigned long constructor)
{
    switch (constructor) {
    case TG_PEER_USER_CONSTRUCTOR:
        return "user";
    case TG_PEER_CHAT_CONSTRUCTOR:
        return "chat";
    case TG_PEER_CHANNEL_CONSTRUCTOR:
        return "channel";
    default:
        return "unknown";
    }
}

tg_mtproto_tl_status tg_mtproto_parse_dialogs_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_dialogs_summary *out)
{
    tg_mtproto_tl_reader reader;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    out->constructor = constructor;
    tg_mtproto_tl_reader_init(&reader, body, body_length);

    if (constructor == TG_MESSAGES_DIALOGS_NOT_MODIFIED_CONSTRUCTOR) {
        out->is_not_modified = 1;
        return tg_mtproto_tl_read_u32(&reader, &out->count);
    }
    if (constructor == TG_MESSAGES_DIALOGS_SLICE_CONSTRUCTOR) {
        out->is_slice = 1;
        if (tg_mtproto_tl_read_u32(&reader, &out->count) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    } else if (constructor != TG_MESSAGES_DIALOGS_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_read_vector_count(&reader, &out->dialog_count) !=
        TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (out->dialog_count != 0UL &&
        (reader.length - reader.offset < 4UL ||
         tg_read_u32_le(reader.buffer + reader.offset) != TG_VECTOR_CONSTRUCTOR)) {
        return TG_MTPROTO_TL_OK;
    }
    if (tg_read_vector_count(&reader, &out->message_count) !=
            TG_MTPROTO_TL_OK ||
        tg_read_vector_count(&reader, &out->chat_count) != TG_MTPROTO_TL_OK ||
        tg_read_vector_count(&reader, &out->user_count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_dialog_peer_list(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_dialog_peer_list *out)
{
    tg_mtproto_tl_reader reader;
    unsigned long i;
    unsigned long count;
    tg_mtproto_dialog_peer peer;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (constructor == TG_MESSAGES_DIALOGS_NOT_MODIFIED_CONSTRUCTOR) {
        return TG_MTPROTO_TL_OK;
    }
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (constructor == TG_MESSAGES_DIALOGS_SLICE_CONSTRUCTOR) {
        if (tg_mtproto_tl_read_u32(&reader, &out->total_dialog_count) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    } else if (constructor != TG_MESSAGES_DIALOGS_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_read_vector_count(&reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor == TG_MESSAGES_DIALOGS_CONSTRUCTOR) {
        out->total_dialog_count = count;
    }
    for (i = 0UL; i < count; ++i) {
        memset(&peer, 0, sizeof(peer));
        if (tg_read_dialog_peer(&reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if (out->count < TG_MTPROTO_DIALOG_PEER_MAX) {
            out->peers[out->count] = peer;
            ++out->count;
        } else {
            out->truncated = 1;
        }
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_peer_cache_entry *tg_peer_cache_find(
    tg_mtproto_peer_cache *cache,
    unsigned long peer_constructor,
    unsigned long id_hi,
    unsigned long id_lo)
{
    unsigned long i;

    for (i = 0UL; i < cache->count; ++i) {
        if (cache->entries[i].peer_constructor == peer_constructor &&
            cache->entries[i].id_hi == id_hi &&
            cache->entries[i].id_lo == id_lo) {
            return &cache->entries[i];
        }
    }
    return 0;
}

static tg_mtproto_peer_cache_entry *tg_peer_cache_add(
    tg_mtproto_peer_cache *cache,
    unsigned long peer_constructor,
    unsigned long id_hi,
    unsigned long id_lo)
{
    tg_mtproto_peer_cache_entry *entry;

    entry = tg_peer_cache_find(cache, peer_constructor, id_hi, id_lo);
    if (entry != 0) {
        return entry;
    }
    if (cache->count >= TG_MTPROTO_PEER_CACHE_MAX) {
        cache->truncated = 1;
        return 0;
    }
    entry = &cache->entries[cache->count++];
    memset(entry, 0, sizeof(*entry));
    entry->peer_constructor = peer_constructor;
    entry->id_hi = id_hi;
    entry->id_lo = id_lo;
    return entry;
}

static void tg_peer_cache_copy_text(char *dst,
                                    unsigned long dst_size,
                                    const char *src)
{
    unsigned long i;

    if (dst == 0 || dst_size == 0UL) {
        return;
    }
    dst[0] = '\0';
    if (src == 0) {
        return;
    }
    for (i = 0UL; i + 1UL < dst_size && src[i] != '\0'; ++i) {
        if (src[i] == '\r' || src[i] == '\n' || src[i] == '\t') {
            dst[i] = ' ';
        } else {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

static void tg_peer_cache_copy_user_title(tg_mtproto_peer_cache_entry *entry,
                                          const tg_mtproto_user_summary *user)
{
    unsigned long pos;
    unsigned long i;

    if (entry == 0 || user == 0) {
        return;
    }
    entry->title[0] = '\0';
    pos = 0UL;
    for (i = 0UL; user->first_name[i] != '\0' &&
         pos + 1UL < sizeof(entry->title); ++i) {
        entry->title[pos++] = user->first_name[i];
    }
    if (pos > 0UL && user->last_name[0] != '\0' &&
        pos + 1UL < sizeof(entry->title)) {
        entry->title[pos++] = ' ';
    }
    for (i = 0UL; user->last_name[i] != '\0' &&
         pos + 1UL < sizeof(entry->title); ++i) {
        entry->title[pos++] = user->last_name[i];
    }
    entry->title[pos] = '\0';
    if (entry->title[0] == '\0') {
        tg_peer_cache_copy_text(entry->title, sizeof(entry->title),
                                user->username);
    }
}

static tg_mtproto_tl_status tg_skip_user_profile_photo(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long scratch_hi;
    unsigned long scratch_lo;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor == 0x4f11bae1UL) {
        return TG_MTPROTO_TL_OK;
    }
    if (constructor != 0x82d1f706UL) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 2UL) != 0UL && tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return tg_mtproto_tl_read_u32(reader, &scratch_lo);
}

static tg_mtproto_tl_status tg_skip_user_status(tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long scratch;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    switch (constructor) {
    case 0x09d05049UL:
    case 0xe26f42f1UL:
    case 0x07bf09fcUL:
    case 0x77ebc742UL:
        return TG_MTPROTO_TL_OK;
    case 0xedb93949UL:
    case 0x008c703fUL:
        return tg_mtproto_tl_read_u32(reader, &scratch);
    default:
        return TG_MTPROTO_TL_INVALID_DATA;
    }
}

static tg_mtproto_tl_status tg_skip_restriction_reason_vector(
    tg_mtproto_tl_reader *reader)
{
    unsigned long count;
    unsigned long i;
    unsigned long constructor;

    if (tg_read_vector_count(reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    for (i = 0UL; i < count; ++i) {
        if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK ||
            constructor != 0xd072acb4UL ||
            tg_skip_string(reader) != TG_MTPROTO_TL_OK ||
            tg_skip_string(reader) != TG_MTPROTO_TL_OK ||
            tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_read_user_summary_from_reader(
    tg_mtproto_tl_reader *reader,
    tg_mtproto_user_summary *out)
{
    memset(out, 0, sizeof(*out));
    if (tg_mtproto_tl_read_u32(reader, &out->constructor) !=
            TG_MTPROTO_TL_OK ||
        out->constructor != TG_USER_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(reader, &out->flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &out->flags2) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u64(reader, &out->id_hi, &out->id_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->is_self = (out->flags & (1UL << 10)) != 0UL;
    out->is_bot = (out->flags & (1UL << 14)) != 0UL;
    if ((out->flags & 1UL) != 0UL) {
        if (tg_mtproto_tl_read_u64(reader, &out->access_hash_hi,
                                   &out->access_hash_lo) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        out->has_access_hash = 1;
    }
    if ((out->flags & 2UL) != 0UL &&
        tg_read_string_copy(reader, out->first_name,
                            sizeof(out->first_name)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 4UL) != 0UL &&
        tg_read_string_copy(reader, out->last_name,
                            sizeof(out->last_name)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 8UL) != 0UL &&
        tg_read_string_copy(reader, out->username,
                            sizeof(out->username)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 16UL) != 0UL &&
        tg_read_string_copy(reader, out->phone,
                            sizeof(out->phone)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 32UL) != 0UL &&
        tg_skip_user_profile_photo(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 64UL) != 0UL &&
        tg_skip_user_status(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & (1UL << 18)) != 0UL &&
        tg_skip_restriction_reason_vector(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & (1UL << 19)) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & (1UL << 22)) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_read_user_leading_from_reader(
    tg_mtproto_tl_reader *reader,
    tg_mtproto_user_summary *out)
{
    memset(out, 0, sizeof(*out));
    if (tg_mtproto_tl_read_u32(reader, &out->constructor) !=
            TG_MTPROTO_TL_OK ||
        out->constructor != TG_USER_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(reader, &out->flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &out->flags2) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u64(reader, &out->id_hi, &out->id_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->is_self = (out->flags & (1UL << 10)) != 0UL;
    out->is_bot = (out->flags & (1UL << 14)) != 0UL;
    if ((out->flags & 1UL) != 0UL) {
        if (tg_mtproto_tl_read_u64(reader, &out->access_hash_hi,
                                   &out->access_hash_lo) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        out->has_access_hash = 1;
    }
    if ((out->flags & 2UL) != 0UL &&
        tg_read_string_copy(reader, out->first_name,
                            sizeof(out->first_name)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 4UL) != 0UL &&
        tg_read_string_copy(reader, out->last_name,
                            sizeof(out->last_name)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 8UL) != 0UL &&
        tg_read_string_copy(reader, out->username,
                            sizeof(out->username)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((out->flags & 16UL) != 0UL &&
        tg_read_string_copy(reader, out->phone,
                            sizeof(out->phone)) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static int tg_peer_cache_apply_user(tg_mtproto_peer_cache *cache,
                                    const tg_mtproto_user_summary *user)
{
    tg_mtproto_peer_cache_entry *entry;

    entry = tg_peer_cache_find(cache, TG_PEER_USER_CONSTRUCTOR,
                               user->id_hi, user->id_lo);
    if (entry == 0 || !entry->from_dialog) {
        return 0;
    }
    entry->has_access_hash = user->has_access_hash;
    entry->access_hash_hi = user->access_hash_hi;
    entry->access_hash_lo = user->access_hash_lo;
    entry->is_self = user->is_self;
    entry->is_bot = user->is_bot;
    tg_peer_cache_copy_user_title(entry, user);
    tg_peer_cache_copy_text(entry->username, sizeof(entry->username),
                            user->username);
    return 1;
}

static unsigned long tg_peer_cache_scan_users(const unsigned char *body,
                                              unsigned long body_length,
                                              tg_mtproto_peer_cache *cache)
{
    tg_mtproto_tl_reader reader;
    tg_mtproto_user_summary user;
    unsigned long offset;
    unsigned long updated;

    if (body == 0 || cache == 0 || body_length < 20UL) {
        return 0UL;
    }
    updated = 0UL;
    for (offset = 0UL; offset + 20UL <= body_length; offset += 4UL) {
        if (tg_read_u32_le(body + offset) != TG_USER_CONSTRUCTOR) {
            continue;
        }
        tg_mtproto_tl_reader_init(&reader, body + offset,
                                  body_length - offset);
        if (tg_read_user_leading_from_reader(&reader, &user) ==
                TG_MTPROTO_TL_OK &&
            tg_peer_cache_apply_user(cache, &user)) {
            ++updated;
        }
    }
    return updated;
}

static int tg_peer_cache_apply_chat(tg_mtproto_peer_cache *cache,
                                    unsigned long peer_constructor,
                                    unsigned long id_hi,
                                    unsigned long id_lo,
                                    unsigned long access_hash_hi,
                                    unsigned long access_hash_lo,
                                    int has_access_hash,
                                    const char *title,
                                    const char *username)
{
    tg_mtproto_peer_cache_entry *entry;

    entry = tg_peer_cache_find(cache, peer_constructor, id_hi, id_lo);
    if (entry == 0 || !entry->from_dialog) {
        return 0;
    }
    entry->has_access_hash = has_access_hash;
    entry->access_hash_hi = access_hash_hi;
    entry->access_hash_lo = access_hash_lo;
    tg_peer_cache_copy_text(entry->title, sizeof(entry->title), title);
    tg_peer_cache_copy_text(entry->username, sizeof(entry->username),
                            username);
    return 1;
}

static tg_mtproto_tl_status tg_read_chat_leading_from_reader(
    tg_mtproto_tl_reader *reader,
    tg_mtproto_peer_cache *cache)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long flags2;
    unsigned long id_hi;
    unsigned long id_lo;
    unsigned long access_hash_hi;
    unsigned long access_hash_lo;
    char title[128];
    char username[96];
    int has_access_hash;

    title[0] = '\0';
    username[0] = '\0';
    access_hash_hi = 0UL;
    access_hash_lo = 0UL;
    has_access_hash = 0;
    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    switch (constructor) {
    case TG_CHAT_CONSTRUCTOR:
        if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u64(reader, &id_hi, &id_lo) !=
                TG_MTPROTO_TL_OK ||
            tg_read_string_copy(reader, title, sizeof(title)) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        (void)flags;
        return tg_peer_cache_apply_chat(cache, TG_PEER_CHAT_CONSTRUCTOR,
                                        id_hi, id_lo, 0UL, 0UL, 0, title,
                                        username) ?
                   TG_MTPROTO_TL_OK :
                   TG_MTPROTO_TL_INVALID_DATA;
    case TG_CHANNEL_CONSTRUCTOR:
        if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &flags2) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u64(reader, &id_hi, &id_lo) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & (1UL << 13)) != 0UL) {
            if (tg_mtproto_tl_read_u64(reader, &access_hash_hi,
                                       &access_hash_lo) !=
                TG_MTPROTO_TL_OK) {
                return TG_MTPROTO_TL_INVALID_DATA;
            }
            has_access_hash = 1;
        }
        if (tg_read_string_copy(reader, title, sizeof(title)) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & (1UL << 6)) != 0UL &&
            tg_read_string_copy(reader, username, sizeof(username)) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        (void)flags2;
        return tg_peer_cache_apply_chat(cache, TG_PEER_CHANNEL_CONSTRUCTOR,
                                        id_hi, id_lo, access_hash_hi,
                                        access_hash_lo, has_access_hash,
                                        title, username) ?
                   TG_MTPROTO_TL_OK :
                   TG_MTPROTO_TL_INVALID_DATA;
    default:
        return TG_MTPROTO_TL_INVALID_DATA;
    }
}

static unsigned long tg_peer_cache_scan_chats(const unsigned char *body,
                                              unsigned long body_length,
                                              tg_mtproto_peer_cache *cache)
{
    tg_mtproto_tl_reader reader;
    unsigned long offset;
    unsigned long constructor;
    unsigned long updated;

    if (body == 0 || cache == 0 || body_length < 20UL) {
        return 0UL;
    }
    updated = 0UL;
    for (offset = 0UL; offset + 20UL <= body_length; offset += 4UL) {
        constructor = tg_read_u32_le(body + offset);
        if (constructor != TG_CHAT_CONSTRUCTOR &&
            constructor != TG_CHANNEL_CONSTRUCTOR) {
            continue;
        }
        tg_mtproto_tl_reader_init(&reader, body + offset,
                                  body_length - offset);
        if (tg_read_chat_leading_from_reader(&reader, cache) ==
            TG_MTPROTO_TL_OK) {
            ++updated;
        }
    }
    return updated;
}

static tg_mtproto_tl_status tg_read_peer_cache_chat(
    tg_mtproto_tl_reader *reader,
    tg_mtproto_peer_cache *cache)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long id_hi;
    unsigned long id_lo;
    unsigned long access_hash_hi;
    unsigned long access_hash_lo;
    tg_mtproto_peer_cache_entry *entry;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    switch (constructor) {
    case TG_CHAT_EMPTY_CONSTRUCTOR:
        if (tg_mtproto_tl_read_u64(reader, &id_hi, &id_lo) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        (void)tg_peer_cache_add(cache, TG_PEER_CHAT_CONSTRUCTOR, id_hi, id_lo);
        return TG_MTPROTO_TL_OK;
    case TG_CHAT_FORBIDDEN_CONSTRUCTOR:
        if (tg_mtproto_tl_read_u64(reader, &id_hi, &id_lo) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        entry = tg_peer_cache_add(cache, TG_PEER_CHAT_CONSTRUCTOR, id_hi,
                                  id_lo);
        if (entry == 0) {
            return tg_skip_string(reader);
        }
        return tg_read_string_copy(reader, entry->title,
                                   sizeof(entry->title));
    case TG_CHANNEL_FORBIDDEN_CONSTRUCTOR:
        if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u64(reader, &id_hi, &id_lo) !=
                TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u64(reader, &access_hash_hi,
                                   &access_hash_lo) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        entry = tg_peer_cache_add(cache, TG_PEER_CHANNEL_CONSTRUCTOR, id_hi,
                                  id_lo);
        if (entry == 0) {
            return tg_skip_string(reader);
        }
        entry->has_access_hash = 1;
        entry->access_hash_hi = access_hash_hi;
        entry->access_hash_lo = access_hash_lo;
        if (tg_read_string_copy(reader, entry->title, sizeof(entry->title)) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & (1UL << 16)) != 0UL &&
            tg_mtproto_tl_read_u32(reader, &id_lo) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        return TG_MTPROTO_TL_OK;
    default:
        return TG_MTPROTO_TL_INVALID_DATA;
    }
}

static tg_mtproto_tl_status tg_skip_message_entity_vector(
    tg_mtproto_tl_reader *reader)
{
    unsigned long count;
    unsigned long constructor;
    unsigned long scratch_hi;
    unsigned long scratch_lo;
    unsigned long i;

    if (tg_read_vector_count(reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    for (i = 0UL; i < count; ++i) {
        if (tg_mtproto_tl_read_u32(reader, &constructor) !=
                TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        switch (constructor) {
        case 0x73924be0UL: /* messageEntityPre */
        case 0x76a6d327UL: /* messageEntityTextUrl */
            if (tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
                return TG_MTPROTO_TL_INVALID_DATA;
            }
            break;
        case 0xdc7b1140UL: /* messageEntityMentionName */
        case 0xc8cf05f8UL: /* messageEntityCustomEmoji */
            if (tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
                TG_MTPROTO_TL_OK) {
                return TG_MTPROTO_TL_INVALID_DATA;
            }
            break;
        case 0xbb92ba95UL: /* messageEntityUnknown */
        case 0xfa04579dUL: /* messageEntityMention */
        case 0x6f635b0dUL: /* messageEntityHashtag */
        case 0x6cef8ac7UL: /* messageEntityBotCommand */
        case 0x6ed02538UL: /* messageEntityUrl */
        case 0x64e475c2UL: /* messageEntityEmail */
        case 0xbd610bc9UL: /* messageEntityBold */
        case 0x826f8b60UL: /* messageEntityItalic */
        case 0x28a20571UL: /* messageEntityCode */
        case 0x9b69e34bUL: /* messageEntityPhone */
        case 0x4c4e743fUL: /* messageEntityCashtag */
        case 0x9c4e7e8bUL: /* messageEntityUnderline */
        case 0xbf0693d4UL: /* messageEntityStrike */
        case 0x20df5d0UL:  /* messageEntityBlockquote */
        case 0x32ca960fUL: /* messageEntitySpoiler */
        case 0x761e6af4UL: /* messageEntityBankCard */
            break;
        default:
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_message_reply_header(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long scratch;
    tg_mtproto_dialog_peer peer;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_MESSAGE_REPLY_HEADER_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 4)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 1UL) != 0UL &&
        tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 5)) != 0UL || (flags & (1UL << 8)) != 0UL) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 2UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 6)) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 7)) != 0UL &&
        tg_skip_message_entity_vector(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 10)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 11)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_message_fwd_header(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long scratch;
    tg_mtproto_dialog_peer peer;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_MESSAGE_FWD_HEADER_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 1UL) != 0UL &&
        tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 5)) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 4UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 8UL) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 4)) != 0UL) {
        if (tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    }
    if ((flags & (1UL << 8)) != 0UL &&
        tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 9)) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 10)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 6)) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_reaction(tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long scratch_hi;
    unsigned long scratch_lo;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    switch (constructor) {
    case 0x79f5d419UL: /* reactionEmpty */
    case 0x523da4ebUL: /* reactionPaid */
        return TG_MTPROTO_TL_OK;
    case 0x1b2286b8UL: /* reactionEmoji */
        return tg_skip_string(reader);
    case 0x8935fc73UL: /* reactionCustomEmoji */
        return tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo);
    default:
        return TG_MTPROTO_TL_INVALID_DATA;
    }
}

static tg_mtproto_tl_status tg_skip_reaction_count_vector(
    tg_mtproto_tl_reader *reader)
{
    unsigned long count;
    unsigned long constructor;
    unsigned long flags;
    unsigned long scratch;
    unsigned long i;

    if (tg_read_vector_count(reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    for (i = 0UL; i < count; ++i) {
        if (tg_mtproto_tl_read_u32(reader, &constructor) !=
                TG_MTPROTO_TL_OK ||
            constructor != TG_REACTION_COUNT_CONSTRUCTOR ||
            tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & 1UL) != 0UL &&
            tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if (tg_skip_reaction(reader) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_message_replies(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long count;
    unsigned long scratch_hi;
    unsigned long scratch_lo;
    unsigned long i;
    tg_mtproto_dialog_peer peer;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_MESSAGE_REPLIES_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 2UL) != 0UL) {
        if (tg_read_vector_count(reader, &count) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        for (i = 0UL; i < count; ++i) {
            if (tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
                return TG_MTPROTO_TL_INVALID_DATA;
            }
        }
    }
    if ((flags & 1UL) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 4UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 8UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_message_reactions(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long flags;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_MESSAGE_REACTIONS_CONSTRUCTOR ||
        tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
        tg_skip_reaction_count_vector(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 2UL) != 0UL || (flags & (1UL << 4)) != 0UL) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_skip_common_message(
    tg_mtproto_tl_reader *reader)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long flags2;
    unsigned long scratch_hi;
    unsigned long scratch_lo;
    tg_mtproto_dialog_peer peer;

    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor == TG_MESSAGE_EMPTY_CONSTRUCTOR) {
        if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & 1UL) != 0UL &&
            tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        return TG_MTPROTO_TL_OK;
    }
    if (constructor == TG_MESSAGE_SERVICE_CONSTRUCTOR) {
        if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & (1UL << 8)) != 0UL &&
            tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if (tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & (1UL << 28)) != 0UL &&
            tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & 8UL) != 0UL) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if (tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor != TG_MESSAGE_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &flags2) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 8)) != 0UL &&
        tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 29)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags2 & 4UL) != 0UL &&
        tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 4UL) != 0UL &&
        tg_skip_message_fwd_header(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 64UL) != 0UL || (flags & 512UL) != 0UL ||
        (flags2 & 8UL) != 0UL || (flags2 & 128UL) != 0UL) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 11)) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags2 & 1UL) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 8UL) != 0UL &&
        tg_skip_message_reply_header(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK ||
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 128UL) != 0UL &&
        tg_skip_message_entity_vector(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 10)) != 0UL) {
        if (tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    }
    if ((flags & (1UL << 23)) != 0UL &&
        tg_skip_message_replies(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 15)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 16)) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 17)) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 20)) != 0UL &&
        tg_skip_message_reactions(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 22)) != 0UL &&
        tg_skip_restriction_reason_vector(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 25)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 30)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags2 & 32UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags2 & 64UL) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_read_common_message_text(
    tg_mtproto_tl_reader *reader,
    tg_mtproto_message_text *out)
{
    unsigned long constructor;
    unsigned long flags;
    unsigned long flags2;
    unsigned long scratch_hi;
    unsigned long scratch_lo;
    tg_mtproto_dialog_peer peer;

    if (out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (tg_mtproto_tl_read_u32(reader, &constructor) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor == TG_MESSAGE_EMPTY_CONSTRUCTOR) {
        if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & 1UL) != 0UL &&
            tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        return TG_MTPROTO_TL_OK;
    }
    if (constructor != TG_MESSAGE_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(reader, &flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &flags2) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(reader, &out->id) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->flags = flags;
    out->is_out = (flags & 2UL) != 0UL;
    if ((flags & (1UL << 8)) != 0UL &&
        tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 29)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags2 & 4UL) != 0UL &&
        tg_read_peer_ref(reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 4UL) != 0UL &&
        tg_skip_message_fwd_header(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & (1UL << 11)) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags2 & 1UL) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if ((flags & 8UL) != 0UL &&
        tg_skip_message_reply_header(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(reader, &out->date) != TG_MTPROTO_TL_OK ||
        tg_read_string_copy(reader, out->text, sizeof(out->text)) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->has_text = out->text[0] != '\0';
    /*
     * Bot replies often carry reply markup or other post-text fields. Keep the
     * message text even when this small reader cannot skip the remaining tail.
     */
    if ((flags & 64UL) != 0UL || (flags & 512UL) != 0UL ||
        (flags2 & 8UL) != 0UL || (flags2 & 128UL) != 0UL) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & 128UL) != 0UL &&
        tg_skip_message_entity_vector(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & (1UL << 10)) != 0UL) {
        if (tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(reader, &scratch_lo) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_OK;
        }
    }
    if ((flags & (1UL << 23)) != 0UL &&
        tg_skip_message_replies(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & (1UL << 15)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & (1UL << 16)) != 0UL &&
        tg_skip_string(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & (1UL << 17)) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & (1UL << 20)) != 0UL &&
        tg_skip_message_reactions(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & (1UL << 22)) != 0UL &&
        tg_skip_restriction_reason_vector(reader) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & (1UL << 25)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags & (1UL << 30)) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags2 & 32UL) != 0UL &&
        tg_mtproto_tl_read_u32(reader, &scratch_lo) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    if ((flags2 & 64UL) != 0UL &&
        tg_mtproto_tl_read_u64(reader, &scratch_hi, &scratch_lo) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_dialog_peer_cache(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_peer_cache *out)
{
    tg_mtproto_tl_reader reader;
    tg_mtproto_dialog_peer peer;
    tg_mtproto_user_summary user;
    tg_mtproto_peer_cache_entry *entry;
    unsigned long count;
    unsigned long i;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (constructor == TG_MESSAGES_DIALOGS_NOT_MODIFIED_CONSTRUCTOR) {
        return TG_MTPROTO_TL_OK;
    }
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (constructor == TG_MESSAGES_DIALOGS_SLICE_CONSTRUCTOR) {
        if (tg_mtproto_tl_read_u32(&reader, &out->total_dialog_count) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    } else if (constructor != TG_MESSAGES_DIALOGS_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_read_vector_count(&reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor == TG_MESSAGES_DIALOGS_CONSTRUCTOR) {
        out->total_dialog_count = count;
    }
    for (i = 0UL; i < count; ++i) {
        memset(&peer, 0, sizeof(peer));
        if (tg_read_dialog_peer(&reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        entry = tg_peer_cache_add(out, peer.peer_constructor, peer.id_hi,
                                  peer.id_lo);
        if (entry != 0) {
            entry->top_message = peer.top_message;
            entry->unread_count = peer.unread_count;
            entry->from_dialog = 1;
        }
    }
    out->user_count = tg_peer_cache_scan_users(body, body_length, out);
    (void)tg_peer_cache_scan_chats(body, body_length, out);
    if (tg_read_vector_count(&reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    for (i = 0UL; i < count; ++i) {
        if (tg_skip_common_message(&reader) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_OK;
        }
    }
    if (tg_read_vector_count(&reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    out->chat_count = count;
    for (i = 0UL; i < count; ++i) {
        if (tg_read_peer_cache_chat(&reader, out) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_OK;
        }
    }
    if (tg_read_vector_count(&reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_OK;
    }
    out->user_count = count;
    for (i = 0UL; i < count; ++i) {
        if (tg_read_user_summary_from_reader(&reader, &user) !=
            TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_OK;
        }
        entry = tg_peer_cache_add(out, TG_PEER_USER_CONSTRUCTOR, user.id_hi,
                                  user.id_lo);
        if (entry != 0) {
            (void)tg_peer_cache_apply_user(out, &user);
        }
    }
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_resolved_peer_cache(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_peer_cache *out)
{
    tg_mtproto_tl_reader reader;
    tg_mtproto_dialog_peer peer;
    tg_mtproto_peer_cache_entry *entry;
    unsigned long i;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (constructor != TG_CONTACTS_RESOLVED_PEER_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    memset(&peer, 0, sizeof(peer));
    if (tg_read_peer_ref(&reader, &peer) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    entry = tg_peer_cache_add(out, peer.peer_constructor, peer.id_hi,
                              peer.id_lo);
    if (entry == 0) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    entry->from_dialog = 1;
    entry->top_message = 0UL;
    entry->unread_count = 0UL;

    (void)tg_peer_cache_scan_chats(body, body_length, out);
    (void)tg_peer_cache_scan_users(body, body_length, out);

    out->user_count = 0UL;
    out->chat_count = 0UL;
    for (i = 0UL; i < out->count; ++i) {
        if (out->entries[i].peer_constructor ==
            TG_PEER_USER_CONSTRUCTOR) {
            ++out->user_count;
        } else if (out->entries[i].peer_constructor ==
                       TG_PEER_CHAT_CONSTRUCTOR ||
                   out->entries[i].peer_constructor ==
                       TG_PEER_CHANNEL_CONSTRUCTOR) {
            ++out->chat_count;
        }
    }
    out->total_dialog_count = out->count;
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_contacts_search_peer_cache(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_peer_cache *out)
{
    tg_mtproto_tl_reader reader;
    tg_mtproto_dialog_peer peer;
    tg_mtproto_peer_cache_entry *entry;
    unsigned long count;
    unsigned long i;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (constructor != TG_CONTACTS_FOUND_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    tg_mtproto_tl_reader_init(&reader, body, body_length);

    if (tg_read_vector_count(&reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    for (i = 0UL; i < count; ++i) {
        memset(&peer, 0, sizeof(peer));
        if (tg_read_peer_ref(&reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        entry = tg_peer_cache_add(out, peer.peer_constructor, peer.id_hi,
                                  peer.id_lo);
        if (entry != 0) {
            entry->from_dialog = 1;
        }
    }

    if (tg_read_vector_count(&reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    for (i = 0UL; i < count; ++i) {
        memset(&peer, 0, sizeof(peer));
        if (tg_read_peer_ref(&reader, &peer) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        entry = tg_peer_cache_add(out, peer.peer_constructor, peer.id_hi,
                                  peer.id_lo);
        if (entry != 0) {
            entry->from_dialog = 1;
        }
    }

    (void)tg_peer_cache_scan_chats(body, body_length, out);
    (void)tg_peer_cache_scan_users(body, body_length, out);

    out->user_count = 0UL;
    out->chat_count = 0UL;
    for (i = 0UL; i < out->count; ++i) {
        if (out->entries[i].peer_constructor == TG_PEER_USER_CONSTRUCTOR) {
            ++out->user_count;
        } else if (out->entries[i].peer_constructor ==
                       TG_PEER_CHAT_CONSTRUCTOR ||
                   out->entries[i].peer_constructor ==
                       TG_PEER_CHANNEL_CONSTRUCTOR) {
            ++out->chat_count;
        }
    }
    out->total_dialog_count = out->count;
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_messages_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_messages_summary *out)
{
    tg_mtproto_tl_reader reader;
    unsigned long scratch;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    out->constructor = constructor;
    tg_mtproto_tl_reader_init(&reader, body, body_length);

    if (constructor == TG_MESSAGES_MESSAGES_NOT_MODIFIED_CONSTRUCTOR) {
        out->is_not_modified = 1;
        return tg_mtproto_tl_read_u32(&reader, &out->count);
    }
    if (constructor == TG_MESSAGES_MESSAGES_SLICE_CONSTRUCTOR) {
        out->is_slice = 1;
        if (tg_mtproto_tl_read_u32(&reader, &out->flags) !=
                TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(&reader, &out->count) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((out->flags & 1UL) != 0UL &&
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((out->flags & 4UL) != 0UL &&
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((out->flags & 8UL) != 0UL &&
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    } else if (constructor == TG_MESSAGES_CHANNEL_MESSAGES_CONSTRUCTOR) {
        out->is_channel_messages = 1;
        if (tg_mtproto_tl_read_u32(&reader, &out->flags) !=
                TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(&reader, &out->count) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((out->flags & 4UL) != 0UL &&
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    } else if (constructor != TG_MESSAGES_MESSAGES_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_read_vector_count(&reader, &out->message_count) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (out->message_count != 0UL &&
        (reader.length - reader.offset < 4UL ||
         tg_read_u32_le(reader.buffer + reader.offset) !=
             TG_VECTOR_CONSTRUCTOR)) {
        return TG_MTPROTO_TL_OK;
    }
    if (constructor == TG_MESSAGES_CHANNEL_MESSAGES_CONSTRUCTOR &&
        tg_read_vector_count(&reader, &scratch) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_read_vector_count(&reader, &out->chat_count) != TG_MTPROTO_TL_OK ||
        tg_read_vector_count(&reader, &out->user_count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_message_text_list(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_message_text_list *out)
{
    tg_mtproto_tl_reader reader;
    tg_mtproto_message_text message;
    unsigned long flags;
    unsigned long count;
    unsigned long scratch;
    unsigned long i;

    if (body == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, body, body_length);

    if (constructor == TG_MESSAGES_MESSAGES_NOT_MODIFIED_CONSTRUCTOR) {
        return TG_MTPROTO_TL_OK;
    }
    if (constructor == TG_MESSAGES_MESSAGES_SLICE_CONSTRUCTOR) {
        if (tg_mtproto_tl_read_u32(&reader, &flags) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(&reader, &out->total_message_count) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & 1UL) != 0UL &&
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & 4UL) != 0UL &&
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & 8UL) != 0UL &&
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    } else if (constructor == TG_MESSAGES_CHANNEL_MESSAGES_CONSTRUCTOR) {
        if (tg_mtproto_tl_read_u32(&reader, &flags) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK ||
            tg_mtproto_tl_read_u32(&reader, &out->total_message_count) !=
                TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
        if ((flags & 4UL) != 0UL &&
            tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK) {
            return TG_MTPROTO_TL_INVALID_DATA;
        }
    } else if (constructor != TG_MESSAGES_MESSAGES_CONSTRUCTOR) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }

    if (tg_read_vector_count(&reader, &count) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (constructor == TG_MESSAGES_MESSAGES_CONSTRUCTOR) {
        out->total_message_count = count;
    }
    for (i = 0UL; i < count; ++i) {
        unsigned long peek_constructor =
            (reader.offset + 4UL <= reader.length) ?
                tg_read_u32_le(reader.buffer + reader.offset) : 0UL;
        if (tg_read_common_message_text(&reader, &message) !=
            TG_MTPROTO_TL_OK) {
            /* Stop here: a TL Message we cannot fully parse cannot be skipped
               (its length is unknown). Record the constructor for diagnosis. */
            out->abort_constructor = peek_constructor;
            return TG_MTPROTO_TL_OK;
        }
        if (message.has_text) {
            if (out->count < TG_MTPROTO_MESSAGE_TEXT_LIST_MAX) {
                out->messages[out->count] = message;
                ++out->count;
            } else {
                out->truncated = 1;
            }
        }
    }
    if (count > TG_MTPROTO_MESSAGE_TEXT_LIST_MAX) {
        out->truncated = 1;
    }
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_updates_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_updates_summary *out)
{
    tg_mtproto_tl_reader reader;
    unsigned long scratch;

    if (out == 0 || (body == 0 && body_length > 0UL)) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    out->constructor = constructor;
    if (constructor != TG_UPDATE_SHORT_SENT_MESSAGE_CONSTRUCTOR) {
        return TG_MTPROTO_TL_OK;
    }
    tg_mtproto_tl_reader_init(&reader, body, body_length);
    if (tg_mtproto_tl_read_u32(&reader, &out->flags) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->id) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &scratch) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &out->date) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->has_sent_message = 1;
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
    static const unsigned char srp_salt1[] = { 0x11U, 0x12U };
    static const unsigned char srp_salt2[] = { 0x21U, 0x22U, 0x23U };
    static const unsigned char srp_p[] = { 0x31U, 0x32U, 0x33U, 0x34U };
    static const unsigned char srp_b[] = { 0x41U, 0x42U, 0x43U };
    static const unsigned char srp_a[] = { 0x51U, 0x52U, 0x53U, 0x54U };
    unsigned char query[128];
    unsigned char initialized[192];
    unsigned char wrapped[160];
    unsigned char rpc[64];
    unsigned char peer_rpc[512];
    unsigned char m1[TG_MTPROTO_SHA256_LENGTH];
    char error_text[32];
    long error_code;
    tg_mtproto_bad_msg_notification bad_msg;
    tg_mtproto_config_summary config;
    tg_mtproto_dialog_peer_list peer_list;
    tg_mtproto_dialogs_summary dialogs;
    tg_mtproto_message_text_list text_list;
    tg_mtproto_peer_cache peer_cache;
    tg_mtproto_messages_summary messages;
    tg_mtproto_password_summary password;
    tg_mtproto_rpc_result result;
    tg_mtproto_sent_code sent_code;
    tg_mtproto_updates_summary updates;
    tg_mtproto_user_summary user;
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
        sent_code.type_length != 5UL ||
        !sent_code.has_type_length ||
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
    if (tg_mtproto_build_users_get_self(&writer) != TG_MTPROTO_TL_OK ||
        writer.length != 16UL ||
        query[0] != 0x48U || query[1] != 0xa5U ||
        query[2] != 0x91U || query[3] != 0x0dU ||
        query[12] != 0x3fU || query[13] != 0xb1U ||
        query[14] != 0xc1U || query[15] != 0xf7U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_contacts_search(&writer, "mario", 10UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 16UL ||
        query[0] != 0xd8U || query[1] != 0x12U ||
        query[2] != 0xf8U || query[3] != 0x11U ||
        query[4] != 5U || memcmp(query + 5U, "mario", 5U) != 0 ||
        query[12] != 10U || query[13] != 0U ||
        query[14] != 0U || query[15] != 0U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_sign_up(&writer, "+1234567890", "hash",
                                      "Amiga", "") != TG_MTPROTO_TL_OK ||
        writer.length != 40UL ||
        query[0] != 0x17U || query[1] != 0xb7U ||
        query[2] != 0xc7U || query[3] != 0xaaU ||
        query[4] != 0x01U || query[5] != 0x00U ||
        query[6] != 0x00U || query[7] != 0x00U) {
        return 2;
    }

    memset(m1, 0x61, sizeof(m1));
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_check_password_empty(&writer) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 8UL ||
        query[0] != 0x16U || query[1] != 0x4dU ||
        query[2] != 0x8bU || query[3] != 0xd1U ||
        query[4] != 0x58U || query[5] != 0xf6U ||
        query[6] != 0x80U || query[7] != 0x98U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_auth_check_password_srp(
            &writer, 0x01020304UL, 0x05060708UL, srp_a, sizeof(srp_a),
            m1) != TG_MTPROTO_TL_OK ||
        writer.length != 60UL ||
        query[0] != 0x16U || query[1] != 0x4dU ||
        query[2] != 0x8bU || query[3] != 0xd1U ||
        query[4] != 0x82U || query[5] != 0xf0U ||
        query[6] != 0x7fU || query[7] != 0xd2U ||
        query[8] != 0x08U || query[9] != 0x07U ||
        query[10] != 0x06U || query[11] != 0x05U ||
        query[12] != 0x04U || query[13] != 0x03U ||
        query[14] != 0x02U || query[15] != 0x01U ||
        query[16] != sizeof(srp_a) ||
        memcmp(query + 17U, srp_a, sizeof(srp_a)) != 0 ||
        query[24] != TG_MTPROTO_SHA256_LENGTH ||
        memcmp(query + 25U, m1, sizeof(m1)) != 0) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_dialogs(&writer, 20UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 32UL ||
        query[0] != 0x4fU || query[1] != 0xcbU ||
        query[2] != 0xf4U || query[3] != 0xa0U ||
        query[16] != 0xeaU || query[17] != 0x18U ||
        query[18] != 0x3bU || query[19] != 0x7fU ||
        query[20] != 20U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_history_self(&writer, 10UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 40UL ||
        query[0] != 0xc5U || query[1] != 0xe6U ||
        query[2] != 0x23U || query[3] != 0x44U ||
        query[4] != 0xc9U || query[5] != 0x7eU ||
        query[6] != 0xa0U || query[7] != 0x7dU ||
        query[20] != 10U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_history_user(
            &writer, 0x01020304UL, 0x05060708UL, 0x11121314UL,
            0x15161718UL, 10UL) != TG_MTPROTO_TL_OK ||
        writer.length != 56UL ||
        query[0] != 0xc5U || query[1] != 0xe6U ||
        query[2] != 0x23U || query[3] != 0x44U ||
        query[4] != 0x4cU || query[5] != 0xa5U ||
        query[6] != 0xe8U || query[7] != 0xddU ||
        query[8] != 0x08U || query[9] != 0x07U ||
        query[10] != 0x06U || query[11] != 0x05U ||
        query[12] != 0x04U || query[13] != 0x03U ||
        query[14] != 0x02U || query[15] != 0x01U ||
        query[16] != 0x18U || query[17] != 0x17U ||
        query[18] != 0x16U || query[19] != 0x15U ||
        query[20] != 0x14U || query[21] != 0x13U ||
        query[22] != 0x12U || query[23] != 0x11U ||
        query[36] != 10U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_history_peer(
            &writer, TG_PEER_CHAT_CONSTRUCTOR, 0x01020304UL, 0x05060708UL,
            0UL, 0UL, 0, 10UL) != TG_MTPROTO_TL_OK ||
        writer.length != 48UL ||
        query[0] != 0xc5U || query[1] != 0xe6U ||
        query[2] != 0x23U || query[3] != 0x44U ||
        query[4] != 0xb9U || query[5] != 0x5cU ||
        query[6] != 0xa9U || query[7] != 0x35U ||
        query[8] != 0x08U || query[9] != 0x07U ||
        query[10] != 0x06U || query[11] != 0x05U ||
        query[12] != 0x04U || query[13] != 0x03U ||
        query[14] != 0x02U || query[15] != 0x01U ||
        query[28] != 10U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_get_history_peer(
            &writer, TG_PEER_CHANNEL_CONSTRUCTOR, 0x01020304UL,
            0x05060708UL, 0x11121314UL, 0x15161718UL, 1, 10UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 56UL ||
        query[4] != 0xfcU || query[5] != 0xbbU ||
        query[6] != 0xbcU || query[7] != 0x27U ||
        query[36] != 10U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_send_self(&writer, "hi", 0x11223344UL,
                                            0x55667788UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 24UL ||
        query[0] != 0x9aU || query[1] != 0xdcU ||
        query[2] != 0x05U || query[3] != 0xfeU ||
        query[8] != 0xc9U || query[9] != 0x7eU ||
        query[10] != 0xa0U || query[11] != 0x7dU ||
        query[16] != 0x88U || query[17] != 0x77U ||
        query[18] != 0x66U || query[19] != 0x55U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_send_user(
            &writer, 0x01020304UL, 0x05060708UL, 0x11121314UL,
            0x15161718UL, "hi", 0x11223344UL, 0x55667788UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 40UL ||
        query[0] != 0x9aU || query[1] != 0xdcU ||
        query[2] != 0x05U || query[3] != 0xfeU ||
        query[8] != 0x4cU || query[9] != 0xa5U ||
        query[10] != 0xe8U || query[11] != 0xddU ||
        query[28] != 0x02U || query[29] != 'h' ||
        query[30] != 'i' ||
        query[32] != 0x88U || query[33] != 0x77U ||
        query[34] != 0x66U || query[35] != 0x55U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_send_peer(
            &writer, TG_PEER_CHAT_CONSTRUCTOR, 0x01020304UL, 0x05060708UL,
            0UL, 0UL, 0, "hi", 0x11223344UL, 0x55667788UL) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 32UL ||
        query[8] != 0xb9U || query[9] != 0x5cU ||
        query[10] != 0xa9U || query[11] != 0x35U ||
        query[20] != 0x02U || query[21] != 'h' ||
        query[22] != 'i' ||
        query[24] != 0x88U || query[25] != 0x77U ||
        query[26] != 0x66U || query[27] != 0x55U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, query, sizeof(query));
    if (tg_mtproto_build_messages_send_peer(
            &writer, TG_PEER_CHANNEL_CONSTRUCTOR, 0x01020304UL,
            0x05060708UL, 0x11121314UL, 0x15161718UL, 1, "hi",
            0x11223344UL, 0x55667788UL) != TG_MTPROTO_TL_OK ||
        writer.length != 40UL ||
        query[8] != 0xfcU || query[9] != 0xbbU ||
        query[10] != 0xbcU || query[11] != 0x27U ||
        query[28] != 0x02U || query[29] != 'h' ||
        query[30] != 'i' ||
        query[32] != 0x88U || query[33] != 0x77U ||
        query[34] != 0x66U || query[35] != 0x55U) {
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
        tg_mtproto_tl_write_u32(&writer, TG_PASSWORD_KDF_ALGO_SRP_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_bytes(&writer, srp_salt1, sizeof(srp_salt1)) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_bytes(&writer, srp_salt2, sizeof(srp_salt2)) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 3UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_bytes(&writer, srp_p, sizeof(srp_p)) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_bytes(&writer, srp_b, sizeof(srp_b)) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x01020304UL, 0x05060708UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_account_password_summary(
            TG_ACCOUNT_PASSWORD_CONSTRUCTOR, rpc, writer.length,
            &password) != TG_MTPROTO_TL_OK ||
        !password.has_recovery || !password.has_secure_values ||
        !password.has_password ||
        !password.has_current_algo ||
        password.current_algo_constructor != TG_PASSWORD_KDF_ALGO_SRP_CONSTRUCTOR ||
        password.current_salt1_length != sizeof(srp_salt1) ||
        password.current_salt2_length != sizeof(srp_salt2) ||
        password.current_p_length != sizeof(srp_p) ||
        password.srp_b_length != sizeof(srp_b) ||
        password.current_g != 3UL ||
        password.srp_id_hi != 0x01020304UL ||
        password.srp_id_lo != 0x05060708UL ||
        memcmp(password.current_salt1, srp_salt1, sizeof(srp_salt1)) != 0 ||
        memcmp(password.current_salt2, srp_salt2, sizeof(srp_salt2)) != 0 ||
        memcmp(password.current_p, srp_p, sizeof(srp_p)) != 0 ||
        memcmp(password.srp_b, srp_b, sizeof(srp_b)) != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 3UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 4UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 5UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_dialogs_summary(TG_MESSAGES_DIALOGS_CONSTRUCTOR,
                                         rpc, writer.length, &dialogs) !=
            TG_MTPROTO_TL_OK ||
        dialogs.dialog_count != 2UL || dialogs.message_count != 3UL ||
        dialogs.chat_count != 4UL || dialogs.user_count != 5UL) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, peer_rpc, sizeof(peer_rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_DIALOG_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 11UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 9UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 10UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 3UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_NOTIFY_SETTINGS_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_DIALOG_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_CHAT_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x87654321UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 21UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 19UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 20UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 4UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_NOTIFY_SETTINGS_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_dialog_peer_list(TG_MESSAGES_DIALOGS_CONSTRUCTOR,
                                          peer_rpc, writer.length,
                                          &peer_list) != TG_MTPROTO_TL_OK ||
        peer_list.count != 2UL || peer_list.total_dialog_count != 2UL ||
        peer_list.peers[0].peer_constructor != TG_PEER_USER_CONSTRUCTOR ||
        peer_list.peers[0].id_lo != 0x12345678UL ||
        peer_list.peers[0].top_message != 11UL ||
        peer_list.peers[0].unread_count != 3UL ||
        peer_list.peers[1].peer_constructor != TG_PEER_CHAT_CONSTRUCTOR ||
        peer_list.peers[1].id_lo != 0x87654321UL ||
        peer_list.peers[1].top_message != 21UL ||
        peer_list.peers[1].unread_count != 4UL) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, peer_rpc, sizeof(peer_rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_DIALOG_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 11UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 9UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 10UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 3UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_NOTIFY_SETTINGS_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_CHAT_FORBIDDEN_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x87654321UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Test Group") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL | 2UL | 4UL | 8UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x01020304UL, 0x05060708UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Ada") != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Lovelace") != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "ada") != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_dialog_peer_cache(TG_MESSAGES_DIALOGS_CONSTRUCTOR,
                                           peer_rpc, writer.length,
                                           &peer_cache) != TG_MTPROTO_TL_OK ||
        peer_cache.count != 2UL || peer_cache.total_dialog_count != 1UL ||
        peer_cache.user_count != 1UL || peer_cache.chat_count != 1UL ||
        !peer_cache.entries[0].has_access_hash ||
        peer_cache.entries[0].access_hash_hi != 0x01020304UL ||
        peer_cache.entries[0].access_hash_lo != 0x05060708UL ||
        strcmp(peer_cache.entries[0].title, "Ada Lovelace") != 0 ||
        strcmp(peer_cache.entries[0].username, "ada") != 0 ||
        peer_cache.entries[1].peer_constructor != TG_PEER_CHAT_CONSTRUCTOR ||
        strcmp(peer_cache.entries[1].title, "Test Group") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, peer_rpc, sizeof(peer_rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_DIALOG_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_CHANNEL_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x2468ace0UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 31UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 29UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 30UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 5UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_NOTIFY_SETTINGS_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_CHANNEL_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL << 13) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x2468ace0UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x11121314UL, 0x15161718UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Test Channel") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_dialog_peer_cache(TG_MESSAGES_DIALOGS_CONSTRUCTOR,
                                           peer_rpc, writer.length,
                                           &peer_cache) != TG_MTPROTO_TL_OK ||
        peer_cache.count != 1UL || peer_cache.chat_count != 1UL ||
        peer_cache.entries[0].peer_constructor != TG_PEER_CHANNEL_CONSTRUCTOR ||
        peer_cache.entries[0].id_lo != 0x2468ace0UL ||
        !peer_cache.entries[0].has_access_hash ||
        peer_cache.entries[0].access_hash_hi != 0x11121314UL ||
        peer_cache.entries[0].access_hash_lo != 0x15161718UL ||
        strcmp(peer_cache.entries[0].title, "Test Channel") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, peer_rpc, sizeof(peer_rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_DIALOG_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 11UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 9UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 10UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 3UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_NOTIFY_SETTINGS_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0xdeadbeefUL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL | 2UL | 8UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x11111111UL, 0x22222222UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Grace") != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "grace") != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_dialog_peer_cache(TG_MESSAGES_DIALOGS_CONSTRUCTOR,
                                           peer_rpc, writer.length,
                                           &peer_cache) != TG_MTPROTO_TL_OK ||
        peer_cache.count != 1UL || peer_cache.user_count != 1UL ||
        !peer_cache.entries[0].has_access_hash ||
        peer_cache.entries[0].access_hash_hi != 0x11111111UL ||
        peer_cache.entries[0].access_hash_lo != 0x22222222UL ||
        strcmp(peer_cache.entries[0].title, "Grace") != 0 ||
        strcmp(peer_cache.entries[0].username, "grace") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, peer_rpc, sizeof(peer_rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_PEER_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL | 2UL | 8UL | 32UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x33333333UL, 0x44444444UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Kaffaine") != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "kaffobot") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0xabcdef01UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_resolved_peer_cache(
            TG_CONTACTS_RESOLVED_PEER_CONSTRUCTOR, peer_rpc, writer.length,
            &peer_cache) != TG_MTPROTO_TL_OK ||
        peer_cache.count != 1UL || peer_cache.user_count != 1UL ||
        !peer_cache.entries[0].has_access_hash ||
        peer_cache.entries[0].access_hash_hi != 0x33333333UL ||
        peer_cache.entries[0].access_hash_lo != 0x44444444UL ||
        strcmp(peer_cache.entries[0].title, "Kaffaine") != 0 ||
        strcmp(peer_cache.entries[0].username, "kaffobot") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, peer_rpc, sizeof(peer_rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_PEER_CHANNEL_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x2468ace0UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_CHANNEL_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL << 13) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x2468ace0UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x11121314UL, 0x15161718UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Resolved Channel") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_resolved_peer_cache(
            TG_CONTACTS_RESOLVED_PEER_CONSTRUCTOR, peer_rpc, writer.length,
            &peer_cache) != TG_MTPROTO_TL_OK ||
        peer_cache.count != 1UL || peer_cache.chat_count != 1UL ||
        peer_cache.entries[0].peer_constructor != TG_PEER_CHANNEL_CONSTRUCTOR ||
        !peer_cache.entries[0].has_access_hash ||
        peer_cache.entries[0].access_hash_hi != 0x11121314UL ||
        peer_cache.entries[0].access_hash_lo != 0x15161718UL ||
        strcmp(peer_cache.entries[0].title, "Resolved Channel") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, peer_rpc, sizeof(peer_rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_CHANNEL_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x2468ace0UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_CHANNEL_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, (1UL << 6) | (1UL << 13)) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x2468ace0UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x11121314UL, 0x15161718UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Amiga Group") != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "amigagroup") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL | 2UL | 8UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x33333333UL, 0x44444444UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Mario") != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "mrossi") != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_contacts_search_peer_cache(
            TG_CONTACTS_FOUND_CONSTRUCTOR, peer_rpc, writer.length,
            &peer_cache) != TG_MTPROTO_TL_OK ||
        peer_cache.count != 2UL || peer_cache.user_count != 1UL ||
        peer_cache.chat_count != 1UL ||
        !peer_cache.entries[0].has_access_hash ||
        peer_cache.entries[0].access_hash_hi != 0x33333333UL ||
        peer_cache.entries[0].access_hash_lo != 0x44444444UL ||
        strcmp(peer_cache.entries[0].title, "Mario") != 0 ||
        strcmp(peer_cache.entries[0].username, "mrossi") != 0 ||
        peer_cache.entries[1].peer_constructor != TG_PEER_CHANNEL_CONSTRUCTOR ||
        !peer_cache.entries[1].has_access_hash ||
        peer_cache.entries[1].access_hash_hi != 0x11121314UL ||
        peer_cache.entries[1].access_hash_lo != 0x15161718UL ||
        strcmp(peer_cache.entries[1].title, "Amiga Group") != 0 ||
        strcmp(peer_cache.entries[1].username, "amigagroup") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 6UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 7UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 8UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_messages_summary(TG_MESSAGES_MESSAGES_CONSTRUCTOR,
                                          rpc, writer.length, &messages) !=
            TG_MTPROTO_TL_OK ||
        messages.message_count != 6UL || messages.chat_count != 7UL ||
        messages.user_count != 8UL) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, peer_rpc, sizeof(peer_rpc));
    if (tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 4UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_MESSAGE_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 4UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1000UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer,
                                TG_MESSAGE_FWD_HEADER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1111UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2221UL) != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "forward reply") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_MESSAGE_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1001UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2222UL) != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "reply text") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_MESSAGE_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1002UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x12345678UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2223UL) != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "my text") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_MESSAGE_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 8UL | 128UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1003UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_PEER_CHAT_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 0x87654321UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer,
                                TG_MESSAGE_REPLY_HEADER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL << 4) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1002UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 2224UL) != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "group reply") != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0xbd610bc9UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 5UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_VECTOR_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_message_text_list(TG_MESSAGES_MESSAGES_CONSTRUCTOR,
                                           peer_rpc, writer.length,
                                           &text_list) != TG_MTPROTO_TL_OK ||
        text_list.count != 4UL || text_list.total_message_count != 4UL ||
        text_list.messages[0].id != 1000UL ||
        text_list.messages[0].date != 2221UL ||
        text_list.messages[0].is_out ||
        strcmp(text_list.messages[0].text, "forward reply") != 0 ||
        text_list.messages[1].id != 1001UL ||
        text_list.messages[1].date != 2222UL ||
        text_list.messages[1].is_out ||
        strcmp(text_list.messages[1].text, "reply text") != 0 ||
        text_list.messages[2].id != 1002UL ||
        text_list.messages[2].date != 2223UL ||
        !text_list.messages[2].is_out ||
        strcmp(text_list.messages[2].text, "my text") != 0 ||
        text_list.messages[3].id != 1003UL ||
        text_list.messages[3].date != 2224UL ||
        text_list.messages[3].is_out ||
        strcmp(text_list.messages[3].text, "group reply") != 0) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 44UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 222UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_updates_summary(
            TG_UPDATE_SHORT_SENT_MESSAGE_CONSTRUCTOR, rpc, writer.length,
            &updates) != TG_MTPROTO_TL_OK ||
        !updates.has_sent_message || updates.id != 44UL ||
        updates.date != 222UL) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, rpc, sizeof(rpc));
    if (tg_mtproto_tl_write_u32(&writer, 1UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_USER_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, (1UL << 10) | 2UL | 8UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0UL, 12345UL) !=
            TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "Test") != TG_MTPROTO_TL_OK ||
        tg_write_string(&writer, "tester") != TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_user_vector_first(TG_VECTOR_CONSTRUCTOR, rpc,
                                           writer.length, &user) !=
            TG_MTPROTO_TL_OK ||
        !user.is_self || user.is_bot || user.id_lo != 12345UL ||
        strcmp(user.first_name, "Test") != 0 ||
        strcmp(user.username, "tester") != 0) {
        return 2;
    }

    return 0;
}
