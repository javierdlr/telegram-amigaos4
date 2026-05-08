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
    const char *telegram_get_me_token_file_path;
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
    int run_telegram_get_me_self_test;
    int run_telegram_get_me;
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
