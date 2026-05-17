/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_MTPROTO_LOGIN_H
#define TG_MTPROTO_LOGIN_H

#include "tg_mtproto_tl.h"

typedef struct tg_mtproto_rpc_result {
    unsigned long request_msg_id_hi;
    unsigned long request_msg_id_lo;
    unsigned long result_constructor;
    const unsigned char *result_body;
    unsigned long result_body_length;
} tg_mtproto_rpc_result;

typedef struct tg_mtproto_bad_msg_notification {
    unsigned long constructor;
    unsigned long bad_msg_id_hi;
    unsigned long bad_msg_id_lo;
    unsigned long bad_msg_seqno;
    unsigned long error_code;
    unsigned long new_server_salt_hi;
    unsigned long new_server_salt_lo;
    int has_new_server_salt;
} tg_mtproto_bad_msg_notification;

typedef struct tg_mtproto_sent_code {
    unsigned long constructor;
    unsigned long type_constructor;
    unsigned long timeout;
    int has_timeout;
    char phone_code_hash[128];
} tg_mtproto_sent_code;

tg_mtproto_tl_status tg_mtproto_build_invoke_with_layer(
    tg_mtproto_tl_writer *writer,
    unsigned long layer,
    const unsigned char *query,
    unsigned long query_length);

tg_mtproto_tl_status tg_mtproto_build_init_connection(
    tg_mtproto_tl_writer *writer,
    unsigned long api_id,
    const char *device_model,
    const char *system_version,
    const char *app_version,
    const char *lang_code,
    const unsigned char *query,
    unsigned long query_length);

tg_mtproto_tl_status tg_mtproto_build_auth_send_code(
    tg_mtproto_tl_writer *writer,
    const char *phone_number,
    unsigned long api_id,
    const char *api_hash);

tg_mtproto_tl_status tg_mtproto_build_auth_sign_in(
    tg_mtproto_tl_writer *writer,
    const char *phone_number,
    const char *phone_code_hash,
    const char *phone_code);

tg_mtproto_tl_status tg_mtproto_parse_rpc_result(
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_rpc_result *out);

tg_mtproto_tl_status tg_mtproto_parse_rpc_error(
    const unsigned char *body,
    unsigned long body_length,
    long *error_code,
    char *error_message,
    unsigned long error_message_size);

tg_mtproto_tl_status tg_mtproto_parse_bad_msg_notification(
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_bad_msg_notification *out);

tg_mtproto_tl_status tg_mtproto_parse_auth_sent_code(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_sent_code *out);

int tg_mtproto_is_auth_authorization_constructor(unsigned long constructor);

int tg_mtproto_login_self_test(void);

#endif
