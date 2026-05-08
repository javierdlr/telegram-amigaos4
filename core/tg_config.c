/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_config.h"
#include "tg_platform.h"

void tg_config_init(tg_config *config)
{
    config->data_dir = tg_platform_default_data_dir();
    config->net_test_host = 0;
    config->net_test_port = 0;
    config->http_test_host = 0;
    config->http_test_port = 0;
    config->http_test_path = 0;
    config->https_test_host = 0;
    config->https_test_port = 0;
    config->https_test_path = 0;
    config->json_test_input = 0;
    config->json_test_field = 0;
    config->telegram_json_test_input = 0;
    config->telegram_path_test_token = 0;
    config->telegram_path_test_method = 0;
    config->telegram_token_file_path = 0;
    config->telegram_token_file_method = 0;
    config->log_level = TG_LOG_INFO;
    config->show_help = 0;
    config->run_net_test = 0;
    config->run_http_test = 0;
    config->run_https_test = 0;
    config->run_json_test = 0;
    config->run_telegram_json_test = 0;
    config->run_telegram_json_self_test = 0;
    config->run_telegram_path_test = 0;
    config->run_telegram_http_self_test = 0;
    config->run_telegram_token_file_path_test = 0;
    config->run_telegram_get_me_self_test = 0;
}

int tg_config_parse(tg_config *config, int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            config->show_help = 1;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            config->log_level = TG_LOG_DEBUG;
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            config->log_level = TG_LOG_WARN;
        } else if (strcmp(argv[i], "--data-dir") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            ++i;
            config->data_dir = argv[i];
        } else if (strcmp(argv[i], "--net-test") == 0) {
            if (i + 2 >= argc) {
                return 1;
            }
            config->run_net_test = 1;
            config->net_test_host = argv[i + 1];
            config->net_test_port = argv[i + 2];
            i += 2;
        } else if (strcmp(argv[i], "--http-test") == 0) {
            if (i + 3 >= argc) {
                return 1;
            }
            config->run_http_test = 1;
            config->http_test_host = argv[i + 1];
            config->http_test_port = argv[i + 2];
            config->http_test_path = argv[i + 3];
            i += 3;
        } else if (strcmp(argv[i], "--https-test") == 0) {
            if (i + 3 >= argc) {
                return 1;
            }
            config->run_https_test = 1;
            config->https_test_host = argv[i + 1];
            config->https_test_port = argv[i + 2];
            config->https_test_path = argv[i + 3];
            i += 3;
        } else if (strcmp(argv[i], "--json-test") == 0) {
            if (i + 2 >= argc) {
                return 1;
            }
            config->run_json_test = 1;
            config->json_test_input = argv[i + 1];
            config->json_test_field = argv[i + 2];
            i += 2;
        } else if (strcmp(argv[i], "--telegram-json-test") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            config->run_telegram_json_test = 1;
            config->telegram_json_test_input = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--telegram-json-self-test") == 0) {
            config->run_telegram_json_self_test = 1;
        } else if (strcmp(argv[i], "--telegram-path-test") == 0) {
            if (i + 2 >= argc) {
                return 1;
            }
            config->run_telegram_path_test = 1;
            config->telegram_path_test_token = argv[i + 1];
            config->telegram_path_test_method = argv[i + 2];
            i += 2;
        } else if (strcmp(argv[i], "--telegram-http-self-test") == 0) {
            config->run_telegram_http_self_test = 1;
        } else if (strcmp(argv[i], "--telegram-token-file-path-test") == 0) {
            if (i + 2 >= argc) {
                return 1;
            }
            config->run_telegram_token_file_path_test = 1;
            config->telegram_token_file_path = argv[i + 1];
            config->telegram_token_file_method = argv[i + 2];
            i += 2;
        } else if (strcmp(argv[i], "--telegram-getme-self-test") == 0) {
            config->run_telegram_get_me_self_test = 1;
        } else {
            return 1;
        }
    }

    return 0;
}

void tg_config_print_usage(FILE *stream, const char *program_name)
{
    fprintf(stream, "Usage: %s [options]\n", program_name);
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -h, --help            Show this help\n");
    fprintf(stream, "  -v, --verbose         Enable debug logging\n");
    fprintf(stream, "  -q, --quiet           Show warnings and errors only\n");
    fprintf(stream, "      --data-dir <path> Set application data directory\n");
    fprintf(stream, "      --net-test <host> <port>\n");
    fprintf(stream, "                         Test DNS resolution and TCP connect\n");
    fprintf(stream, "      --http-test <host> <port> <path>\n");
    fprintf(stream, "                         Test TCP send and receive with HTTP/1.0\n");
    fprintf(stream, "      --https-test <host> <port> <path>\n");
    fprintf(stream, "                         Test TLS send and receive with HTTP/1.0\n");
    fprintf(stream, "      --json-test <json> <field>\n");
    fprintf(stream, "                         Test top-level JSON field lookup\n");
    fprintf(stream, "      --telegram-json-test <json>\n");
    fprintf(stream, "                         Test Telegram API response parsing\n");
    fprintf(stream, "      --telegram-json-self-test\n");
    fprintf(stream, "                         Run built-in Telegram JSON parser samples\n");
    fprintf(stream, "      --telegram-path-test <token> <method>\n");
    fprintf(stream, "                         Test Telegram Bot API path construction\n");
    fprintf(stream, "      --telegram-http-self-test\n");
    fprintf(stream, "                         Run built-in HTTP-to-Telegram parser samples\n");
    fprintf(stream, "      --telegram-token-file-path-test <file> <method>\n");
    fprintf(stream, "                         Load token file and test Bot API path construction\n");
    fprintf(stream, "      --telegram-getme-self-test\n");
    fprintf(stream, "                         Run built-in Bot API getMe parser sample\n");
}
