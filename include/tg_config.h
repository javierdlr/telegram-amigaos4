/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_CONFIG_H
#define TG_CONFIG_H

#include <stdio.h>

#include "tg_log.h"

/**
 * Parsed command-line configuration.
 *
 * String fields point directly into argv or static platform strings. The config
 * object does not allocate or own those strings.
 */
typedef struct tg_config {
    const char *data_dir;
    const char *token_file_path_override;
    const char *net_test_host;
    const char *net_test_port;
    const char *http_test_host;
    const char *http_test_port;
    const char *http_test_path;
    const char *https_test_host;
    const char *https_test_port;
    const char *https_test_path;
    const char *json_test_input;
    const char *json_test_field;
    const char *telegram_json_test_input;
    const char *telegram_path_test_token;
    const char *telegram_path_test_method;
    const char *telegram_token_file_path;
    const char *telegram_token_file_method;
    const char *telegram_default_token_file_method;
    const char *telegram_get_me_token_file_path;
    const char *telegram_get_updates_token_file_path;
    const char *telegram_get_updates_offset;
    const char *telegram_get_updates_default_offset;
    const char *telegram_read_once_state_token_file_path;
    const char *telegram_read_once_state_offset_file_path;
    const char *telegram_read_once_state_default_offset_file_path;
    const char *telegram_read_loop_token_file_path;
    const char *telegram_read_loop_offset_file_path;
    const char *telegram_read_loop_poll_seconds;
    const char *telegram_read_loop_max_iterations;
    const char *telegram_read_loop_default_offset_file_path;
    const char *telegram_read_loop_default_poll_seconds;
    const char *telegram_read_loop_default_max_iterations;
    const char *telegram_inbox_token_file_path;
    const char *telegram_inbox_offset_file_path;
    const char *telegram_inbox_default_offset_file_path;
    const char *telegram_inbox_loop_token_file_path;
    const char *telegram_inbox_loop_offset_file_path;
    const char *telegram_inbox_loop_poll_seconds;
    const char *telegram_inbox_loop_max_iterations;
    const char *telegram_inbox_loop_default_offset_file_path;
    const char *telegram_inbox_loop_default_poll_seconds;
    const char *telegram_inbox_loop_default_max_iterations;
    const char *telegram_echo_once_token_file_path;
    const char *telegram_echo_once_offset;
    const char *telegram_echo_once_default_offset;
    const char *telegram_echo_once_state_token_file_path;
    const char *telegram_echo_once_state_offset_file_path;
    const char *telegram_echo_once_state_default_offset_file_path;
    const char *telegram_echo_loop_token_file_path;
    const char *telegram_echo_loop_offset_file_path;
    const char *telegram_echo_loop_poll_seconds;
    const char *telegram_echo_loop_max_iterations;
    const char *telegram_echo_loop_default_offset_file_path;
    const char *telegram_echo_loop_default_poll_seconds;
    const char *telegram_echo_loop_default_max_iterations;
    const char *telegram_send_message_token_file_path;
    const char *telegram_send_message_chat_id;
    const char *telegram_send_message_text;
    const char *telegram_send_message_default_chat_id;
    const char *telegram_send_message_default_text;
    tg_log_level log_level;
    int show_help;
    int run_net_test;
    int run_http_test;
    int run_http_post_self_test;
    int run_https_test;
    int run_json_test;
    int run_telegram_json_test;
    int run_telegram_json_self_test;
    int run_telegram_path_test;
    int run_telegram_http_self_test;
    int run_telegram_token_file_path_test;
    int run_telegram_default_token_file_path_test;
    int run_telegram_preflight;
    int run_telegram_get_me_self_test;
    int run_telegram_get_me;
    int run_telegram_get_me_default;
    int run_telegram_get_updates_self_test;
    int run_telegram_get_updates;
    int run_telegram_get_updates_default;
    int run_telegram_read_once_state_self_test;
    int run_telegram_read_once_state;
    int run_telegram_read_once_state_default;
    int run_telegram_read_loop;
    int run_telegram_read_loop_default;
    int run_telegram_inbox_self_test;
    int run_telegram_inbox;
    int run_telegram_inbox_default;
    int run_telegram_inbox_loop;
    int run_telegram_inbox_loop_default;
    int run_telegram_echo_once_self_test;
    int run_telegram_echo_once;
    int run_telegram_echo_once_default;
    int run_telegram_echo_once_state;
    int run_telegram_echo_once_state_default;
    int run_telegram_echo_loop;
    int run_telegram_echo_loop_default;
    int run_telegram_send_message_self_test;
    int run_telegram_send_message;
    int run_telegram_send_message_default;
} tg_config;

/**
 * Initializes config with platform defaults and all test flags disabled.
 */
void tg_config_init(tg_config *config);

/**
 * Parses command-line options into config.
 *
 * No memory is allocated. String options are stored as pointers to argv values.
 * Returns 0 on success and non-zero on invalid or incomplete arguments.
 */
int tg_config_parse(tg_config *config, int argc, char **argv);

/**
 * Prints supported command-line options to stream.
 */
void tg_config_print_usage(FILE *stream, const char *program_name);

#endif
