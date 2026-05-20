/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_MTPROTO_LOGIN_H
#define TG_MTPROTO_LOGIN_H

#include "tg_mtproto_crypto.h"
#include "tg_mtproto_tl.h"

#define TG_MTPROTO_PASSWORD_BYTES_MAX 256U

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
    unsigned long type_length;
    unsigned long timeout;
    int has_timeout;
    int has_type_length;
    char phone_code_hash[128];
} tg_mtproto_sent_code;

typedef struct tg_mtproto_config_summary {
    unsigned long date;
    unsigned long expires;
    unsigned long test_mode_constructor;
    unsigned long this_dc;
} tg_mtproto_config_summary;

typedef struct tg_mtproto_password_summary {
    unsigned long flags;
    unsigned long current_algo_constructor;
    unsigned long current_g;
    unsigned long current_salt1_length;
    unsigned long current_salt2_length;
    unsigned long current_p_length;
    unsigned long srp_b_length;
    unsigned long srp_id_hi;
    unsigned long srp_id_lo;
    int has_recovery;
    int has_secure_values;
    int has_password;
    int has_current_algo;
    unsigned char current_salt1[TG_MTPROTO_PASSWORD_BYTES_MAX];
    unsigned char current_salt2[TG_MTPROTO_PASSWORD_BYTES_MAX];
    unsigned char current_p[TG_MTPROTO_PASSWORD_BYTES_MAX];
    unsigned char srp_b[TG_MTPROTO_PASSWORD_BYTES_MAX];
} tg_mtproto_password_summary;

typedef struct tg_mtproto_user_summary {
    unsigned long constructor;
    unsigned long flags;
    unsigned long flags2;
    unsigned long id_hi;
    unsigned long id_lo;
    unsigned long access_hash_hi;
    unsigned long access_hash_lo;
    int has_access_hash;
    int is_self;
    int is_bot;
    char first_name[96];
    char last_name[96];
    char username[96];
    char phone[64];
} tg_mtproto_user_summary;

typedef struct tg_mtproto_dialogs_summary {
    unsigned long constructor;
    unsigned long count;
    unsigned long dialog_count;
    unsigned long message_count;
    unsigned long chat_count;
    unsigned long user_count;
    int is_slice;
    int is_not_modified;
} tg_mtproto_dialogs_summary;

#define TG_MTPROTO_DIALOG_PEER_MAX 32U

typedef struct tg_mtproto_dialog_peer {
    unsigned long peer_constructor;
    unsigned long id_hi;
    unsigned long id_lo;
    unsigned long top_message;
    unsigned long unread_count;
} tg_mtproto_dialog_peer;

typedef struct tg_mtproto_dialog_peer_list {
    unsigned long count;
    unsigned long total_dialog_count;
    int truncated;
    tg_mtproto_dialog_peer peers[TG_MTPROTO_DIALOG_PEER_MAX];
} tg_mtproto_dialog_peer_list;

#define TG_MTPROTO_PEER_CACHE_MAX 64U

typedef struct tg_mtproto_peer_cache_entry {
    unsigned long peer_constructor;
    unsigned long id_hi;
    unsigned long id_lo;
    unsigned long access_hash_hi;
    unsigned long access_hash_lo;
    unsigned long top_message;
    unsigned long unread_count;
    int has_access_hash;
    int is_self;
    int is_bot;
    int from_dialog;
    char title[128];
    char username[96];
} tg_mtproto_peer_cache_entry;

typedef struct tg_mtproto_peer_cache {
    unsigned long count;
    unsigned long total_dialog_count;
    unsigned long user_count;
    unsigned long chat_count;
    int truncated;
    tg_mtproto_peer_cache_entry entries[TG_MTPROTO_PEER_CACHE_MAX];
} tg_mtproto_peer_cache;

typedef struct tg_mtproto_messages_summary {
    unsigned long constructor;
    unsigned long flags;
    unsigned long count;
    unsigned long message_count;
    unsigned long chat_count;
    unsigned long user_count;
    int is_slice;
    int is_not_modified;
    int is_channel_messages;
} tg_mtproto_messages_summary;

#define TG_MTPROTO_MESSAGE_TEXT_MAX 256U
#define TG_MTPROTO_MESSAGE_TEXT_LIST_MAX 8U

typedef struct tg_mtproto_message_text {
    unsigned long id;
    unsigned long date;
    unsigned long flags;
    int is_out;
    int has_text;
    char text[TG_MTPROTO_MESSAGE_TEXT_MAX];
} tg_mtproto_message_text;

typedef struct tg_mtproto_message_text_list {
    unsigned long count;
    unsigned long total_message_count;
    int truncated;
    tg_mtproto_message_text messages[TG_MTPROTO_MESSAGE_TEXT_LIST_MAX];
} tg_mtproto_message_text_list;

typedef struct tg_mtproto_updates_summary {
    unsigned long constructor;
    unsigned long flags;
    unsigned long id;
    unsigned long date;
    int has_sent_message;
} tg_mtproto_updates_summary;

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

tg_mtproto_tl_status tg_mtproto_build_auth_sign_up(
    tg_mtproto_tl_writer *writer,
    const char *phone_number,
    const char *phone_code_hash,
    const char *first_name,
    const char *last_name);

tg_mtproto_tl_status tg_mtproto_build_input_check_password_empty(
    tg_mtproto_tl_writer *writer);

tg_mtproto_tl_status tg_mtproto_build_input_check_password_srp(
    tg_mtproto_tl_writer *writer,
    unsigned long srp_id_hi,
    unsigned long srp_id_lo,
    const unsigned char *a,
    unsigned long a_length,
    const unsigned char m1[TG_MTPROTO_SHA256_LENGTH]);

tg_mtproto_tl_status tg_mtproto_build_auth_check_password_empty(
    tg_mtproto_tl_writer *writer);

tg_mtproto_tl_status tg_mtproto_build_auth_check_password_srp(
    tg_mtproto_tl_writer *writer,
    unsigned long srp_id_hi,
    unsigned long srp_id_lo,
    const unsigned char *a,
    unsigned long a_length,
    const unsigned char m1[TG_MTPROTO_SHA256_LENGTH]);

tg_mtproto_tl_status tg_mtproto_build_help_get_config(
    tg_mtproto_tl_writer *writer);

tg_mtproto_tl_status tg_mtproto_build_account_get_password(
    tg_mtproto_tl_writer *writer);

tg_mtproto_tl_status tg_mtproto_build_users_get_self(
    tg_mtproto_tl_writer *writer);

tg_mtproto_tl_status tg_mtproto_build_messages_get_dialogs(
    tg_mtproto_tl_writer *writer,
    unsigned long limit);

tg_mtproto_tl_status tg_mtproto_build_messages_get_history_self(
    tg_mtproto_tl_writer *writer,
    unsigned long limit);

tg_mtproto_tl_status tg_mtproto_build_messages_get_history_user(
    tg_mtproto_tl_writer *writer,
    unsigned long user_id_hi,
    unsigned long user_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo,
    unsigned long limit);

tg_mtproto_tl_status tg_mtproto_build_messages_send_self(
    tg_mtproto_tl_writer *writer,
    const char *message,
    unsigned long random_id_hi,
    unsigned long random_id_lo);

tg_mtproto_tl_status tg_mtproto_build_messages_send_user(
    tg_mtproto_tl_writer *writer,
    unsigned long user_id_hi,
    unsigned long user_id_lo,
    unsigned long access_hash_hi,
    unsigned long access_hash_lo,
    const char *message,
    unsigned long random_id_hi,
    unsigned long random_id_lo);

tg_mtproto_tl_status tg_mtproto_build_msgs_ack(
    tg_mtproto_tl_writer *writer,
    const unsigned long *msg_id_hi,
    const unsigned long *msg_id_lo,
    unsigned long msg_id_count);

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

tg_mtproto_tl_status tg_mtproto_parse_config_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_config_summary *out);

tg_mtproto_tl_status tg_mtproto_parse_account_password_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_password_summary *out);

tg_mtproto_tl_status tg_mtproto_parse_user_vector_first(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_user_summary *out);

tg_mtproto_tl_status tg_mtproto_parse_dialogs_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_dialogs_summary *out);

tg_mtproto_tl_status tg_mtproto_parse_dialog_peer_list(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_dialog_peer_list *out);

tg_mtproto_tl_status tg_mtproto_parse_dialog_peer_cache(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_peer_cache *out);

const char *tg_mtproto_peer_constructor_name(unsigned long constructor);

tg_mtproto_tl_status tg_mtproto_parse_messages_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_messages_summary *out);

tg_mtproto_tl_status tg_mtproto_parse_message_text_list(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_message_text_list *out);

tg_mtproto_tl_status tg_mtproto_parse_updates_summary(
    unsigned long constructor,
    const unsigned char *body,
    unsigned long body_length,
    tg_mtproto_updates_summary *out);

int tg_mtproto_is_auth_authorization_constructor(unsigned long constructor);

int tg_mtproto_login_self_test(void);

#endif
