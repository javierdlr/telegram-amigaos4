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
    config->token_file_path_override = 0;
    config->inbox_log_file_path = 0;
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
    config->telegram_default_token_file_method = 0;
    config->telegram_get_me_token_file_path = 0;
    config->telegram_get_updates_token_file_path = 0;
    config->telegram_get_updates_offset = 0;
    config->telegram_get_updates_default_offset = 0;
    config->telegram_read_once_state_token_file_path = 0;
    config->telegram_read_once_state_offset_file_path = 0;
    config->telegram_read_once_state_default_offset_file_path = 0;
    config->telegram_read_loop_token_file_path = 0;
    config->telegram_read_loop_offset_file_path = 0;
    config->telegram_read_loop_poll_seconds = 0;
    config->telegram_read_loop_max_iterations = 0;
    config->telegram_read_loop_default_offset_file_path = 0;
    config->telegram_read_loop_default_poll_seconds = 0;
    config->telegram_read_loop_default_max_iterations = 0;
    config->telegram_inbox_token_file_path = 0;
    config->telegram_inbox_offset_file_path = 0;
    config->telegram_inbox_default_offset_file_path = 0;
    config->telegram_inbox_loop_token_file_path = 0;
    config->telegram_inbox_loop_offset_file_path = 0;
    config->telegram_inbox_loop_poll_seconds = 0;
    config->telegram_inbox_loop_max_iterations = 0;
    config->telegram_inbox_loop_default_offset_file_path = 0;
    config->telegram_inbox_loop_default_poll_seconds = 0;
    config->telegram_inbox_loop_default_max_iterations = 0;
    config->telegram_echo_once_token_file_path = 0;
    config->telegram_echo_once_offset = 0;
    config->telegram_echo_once_default_offset = 0;
    config->telegram_echo_once_state_token_file_path = 0;
    config->telegram_echo_once_state_offset_file_path = 0;
    config->telegram_echo_once_state_default_offset_file_path = 0;
    config->telegram_echo_loop_token_file_path = 0;
    config->telegram_echo_loop_offset_file_path = 0;
    config->telegram_echo_loop_poll_seconds = 0;
    config->telegram_echo_loop_max_iterations = 0;
    config->telegram_echo_loop_default_offset_file_path = 0;
    config->telegram_echo_loop_default_poll_seconds = 0;
    config->telegram_echo_loop_default_max_iterations = 0;
    config->telegram_send_message_token_file_path = 0;
    config->telegram_send_message_chat_id = 0;
    config->telegram_send_message_text = 0;
    config->telegram_send_message_default_chat_id = 0;
    config->telegram_send_message_default_text = 0;
    config->log_level = TG_LOG_INFO;
    config->show_help = 0;
    config->run_net_test = 0;
    config->run_http_test = 0;
    config->run_http_post_self_test = 0;
    config->run_https_test = 0;
    config->run_json_test = 0;
    config->run_telegram_json_test = 0;
    config->run_telegram_json_self_test = 0;
    config->run_telegram_path_test = 0;
    config->run_telegram_http_self_test = 0;
    config->run_telegram_token_file_path_test = 0;
    config->run_telegram_default_token_file_path_test = 0;
    config->run_telegram_preflight = 0;
    config->run_telegram_get_me_self_test = 0;
    config->run_telegram_get_me = 0;
    config->run_telegram_get_me_default = 0;
    config->run_telegram_get_updates_self_test = 0;
    config->run_telegram_get_updates = 0;
    config->run_telegram_get_updates_default = 0;
    config->run_telegram_read_once_state_self_test = 0;
    config->run_telegram_read_once_state = 0;
    config->run_telegram_read_once_state_default = 0;
    config->run_telegram_read_loop = 0;
    config->run_telegram_read_loop_default = 0;
    config->run_telegram_inbox_self_test = 0;
    config->run_telegram_inbox = 0;
    config->run_telegram_inbox_default = 0;
    config->run_telegram_inbox_loop = 0;
    config->run_telegram_inbox_loop_default = 0;
    config->run_telegram_echo_once_self_test = 0;
    config->run_telegram_echo_once = 0;
    config->run_telegram_echo_once_default = 0;
    config->run_telegram_echo_once_state = 0;
    config->run_telegram_echo_once_state_default = 0;
    config->run_telegram_echo_loop = 0;
    config->run_telegram_echo_loop_default = 0;
    config->run_telegram_send_message_self_test = 0;
    config->run_telegram_send_message = 0;
    config->run_telegram_send_message_default = 0;
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
        } else if (strcmp(argv[i], "--token-file") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            ++i;
            config->token_file_path_override = argv[i];
        } else if (strcmp(argv[i], "--inbox-log-file") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            ++i;
            config->inbox_log_file_path = argv[i];
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
        } else if (strcmp(argv[i], "--http-post-self-test") == 0) {
            config->run_http_post_self_test = 1;
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
        } else if (strcmp(argv[i], "--telegram-default-token-file-path-test") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            config->run_telegram_default_token_file_path_test = 1;
            config->telegram_default_token_file_method = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--telegram-preflight") == 0) {
            config->run_telegram_preflight = 1;
        } else if (strcmp(argv[i], "--telegram-getme-self-test") == 0) {
            config->run_telegram_get_me_self_test = 1;
        } else if (strcmp(argv[i], "--telegram-getme") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            config->run_telegram_get_me = 1;
            config->telegram_get_me_token_file_path = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--telegram-getme-default") == 0) {
            config->run_telegram_get_me_default = 1;
        } else if (strcmp(argv[i], "--telegram-get-updates-self-test") == 0) {
            config->run_telegram_get_updates_self_test = 1;
        } else if (strcmp(argv[i], "--telegram-get-updates") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            config->run_telegram_get_updates = 1;
            config->telegram_get_updates_token_file_path = argv[i + 1];
            ++i;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                config->telegram_get_updates_offset = argv[i + 1];
                ++i;
            }
        } else if (strcmp(argv[i], "--telegram-get-updates-default") == 0) {
            config->run_telegram_get_updates_default = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                config->telegram_get_updates_default_offset = argv[i + 1];
                ++i;
            }
        } else if (strcmp(argv[i], "--telegram-read-once-state-self-test") == 0) {
            config->run_telegram_read_once_state_self_test = 1;
        } else if (strcmp(argv[i], "--telegram-read-once-state") == 0) {
            if (i + 2 >= argc) {
                return 1;
            }
            config->run_telegram_read_once_state = 1;
            config->telegram_read_once_state_token_file_path = argv[i + 1];
            config->telegram_read_once_state_offset_file_path = argv[i + 2];
            i += 2;
        } else if (strcmp(argv[i], "--telegram-read-once-state-default") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            config->run_telegram_read_once_state_default = 1;
            config->telegram_read_once_state_default_offset_file_path = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--telegram-read-loop") == 0) {
            if (i + 4 >= argc) {
                return 1;
            }
            config->run_telegram_read_loop = 1;
            config->telegram_read_loop_token_file_path = argv[i + 1];
            config->telegram_read_loop_offset_file_path = argv[i + 2];
            config->telegram_read_loop_poll_seconds = argv[i + 3];
            config->telegram_read_loop_max_iterations = argv[i + 4];
            i += 4;
        } else if (strcmp(argv[i], "--telegram-read-loop-default") == 0) {
            if (i + 3 >= argc) {
                return 1;
            }
            config->run_telegram_read_loop_default = 1;
            config->telegram_read_loop_default_offset_file_path = argv[i + 1];
            config->telegram_read_loop_default_poll_seconds = argv[i + 2];
            config->telegram_read_loop_default_max_iterations = argv[i + 3];
            i += 3;
        } else if (strcmp(argv[i], "--telegram-inbox-self-test") == 0) {
            config->run_telegram_inbox_self_test = 1;
        } else if (strcmp(argv[i], "--telegram-inbox") == 0) {
            if (i + 2 >= argc) {
                return 1;
            }
            config->run_telegram_inbox = 1;
            config->telegram_inbox_token_file_path = argv[i + 1];
            config->telegram_inbox_offset_file_path = argv[i + 2];
            i += 2;
        } else if (strcmp(argv[i], "--telegram-inbox-default") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            config->run_telegram_inbox_default = 1;
            config->telegram_inbox_default_offset_file_path = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--telegram-inbox-loop") == 0) {
            if (i + 4 >= argc) {
                return 1;
            }
            config->run_telegram_inbox_loop = 1;
            config->telegram_inbox_loop_token_file_path = argv[i + 1];
            config->telegram_inbox_loop_offset_file_path = argv[i + 2];
            config->telegram_inbox_loop_poll_seconds = argv[i + 3];
            config->telegram_inbox_loop_max_iterations = argv[i + 4];
            i += 4;
        } else if (strcmp(argv[i], "--telegram-inbox-loop-default") == 0) {
            if (i + 3 >= argc) {
                return 1;
            }
            config->run_telegram_inbox_loop_default = 1;
            config->telegram_inbox_loop_default_offset_file_path = argv[i + 1];
            config->telegram_inbox_loop_default_poll_seconds = argv[i + 2];
            config->telegram_inbox_loop_default_max_iterations = argv[i + 3];
            i += 3;
        } else if (strcmp(argv[i], "--telegram-echo-once-self-test") == 0) {
            config->run_telegram_echo_once_self_test = 1;
        } else if (strcmp(argv[i], "--telegram-echo-once") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            config->run_telegram_echo_once = 1;
            config->telegram_echo_once_token_file_path = argv[i + 1];
            ++i;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                config->telegram_echo_once_offset = argv[i + 1];
                ++i;
            }
        } else if (strcmp(argv[i], "--telegram-echo-once-default") == 0) {
            config->run_telegram_echo_once_default = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                config->telegram_echo_once_default_offset = argv[i + 1];
                ++i;
            }
        } else if (strcmp(argv[i], "--telegram-echo-once-state") == 0) {
            if (i + 2 >= argc) {
                return 1;
            }
            config->run_telegram_echo_once_state = 1;
            config->telegram_echo_once_state_token_file_path = argv[i + 1];
            config->telegram_echo_once_state_offset_file_path = argv[i + 2];
            i += 2;
        } else if (strcmp(argv[i], "--telegram-echo-once-state-default") == 0) {
            if (i + 1 >= argc) {
                return 1;
            }
            config->run_telegram_echo_once_state_default = 1;
            config->telegram_echo_once_state_default_offset_file_path = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--telegram-echo-loop") == 0) {
            if (i + 4 >= argc) {
                return 1;
            }
            config->run_telegram_echo_loop = 1;
            config->telegram_echo_loop_token_file_path = argv[i + 1];
            config->telegram_echo_loop_offset_file_path = argv[i + 2];
            config->telegram_echo_loop_poll_seconds = argv[i + 3];
            config->telegram_echo_loop_max_iterations = argv[i + 4];
            i += 4;
        } else if (strcmp(argv[i], "--telegram-echo-loop-default") == 0) {
            if (i + 3 >= argc) {
                return 1;
            }
            config->run_telegram_echo_loop_default = 1;
            config->telegram_echo_loop_default_offset_file_path = argv[i + 1];
            config->telegram_echo_loop_default_poll_seconds = argv[i + 2];
            config->telegram_echo_loop_default_max_iterations = argv[i + 3];
            i += 3;
        } else if (strcmp(argv[i], "--telegram-send-message-self-test") == 0) {
            config->run_telegram_send_message_self_test = 1;
        } else if (strcmp(argv[i], "--telegram-send-message") == 0 ||
                   strcmp(argv[i], "--telegram-send") == 0) {
            if (i + 3 >= argc) {
                return 1;
            }
            config->run_telegram_send_message = 1;
            config->telegram_send_message_token_file_path = argv[i + 1];
            config->telegram_send_message_chat_id = argv[i + 2];
            config->telegram_send_message_text = argv[i + 3];
            i += 3;
        } else if (strcmp(argv[i], "--telegram-send-message-default") == 0 ||
                   strcmp(argv[i], "--telegram-send-default") == 0) {
            if (i + 2 >= argc) {
                return 1;
            }
            config->run_telegram_send_message_default = 1;
            config->telegram_send_message_default_chat_id = argv[i + 1];
            config->telegram_send_message_default_text = argv[i + 2];
            i += 2;
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
    fprintf(stream, "      --token-file <path>\n");
    fprintf(stream, "                         Override default Telegram token file\n");
    fprintf(stream, "      --inbox-log-file <path>\n");
    fprintf(stream, "                         Append read-only inbox items to a local text log\n");
    fprintf(stream, "      --net-test <host> <port>\n");
    fprintf(stream, "                         Test DNS resolution and TCP connect\n");
    fprintf(stream, "      --http-test <host> <port> <path>\n");
    fprintf(stream, "                         Test TCP send and receive with HTTP/1.0\n");
    fprintf(stream, "      --http-post-self-test\n");
    fprintf(stream, "                         Run built-in HTTP POST request builder sample\n");
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
    fprintf(stream, "      --telegram-default-token-file-path-test <method>\n");
    fprintf(stream, "                         Load default token file and test Bot API path construction\n");
    fprintf(stream, "      --telegram-preflight\n");
    fprintf(stream, "                         Check token path and Telegram HTTPS reachability\n");
    fprintf(stream, "      --telegram-getme-self-test\n");
    fprintf(stream, "                         Run built-in Bot API getMe parser sample\n");
    fprintf(stream, "      --telegram-getme <file>\n");
    fprintf(stream, "                         Call Telegram getMe with token loaded from file\n");
    fprintf(stream, "      --telegram-getme-default\n");
    fprintf(stream, "                         Call Telegram getMe with default token file\n");
    fprintf(stream, "      --telegram-get-updates-self-test\n");
    fprintf(stream, "                         Run built-in Bot API getUpdates parser sample\n");
    fprintf(stream, "      --telegram-get-updates <file> [offset]\n");
    fprintf(stream, "                         Call Telegram getUpdates with optional offset\n");
    fprintf(stream, "      --telegram-get-updates-default [offset]\n");
    fprintf(stream, "                         Call Telegram getUpdates with default token file\n");
    fprintf(stream, "      --telegram-read-once-state-self-test\n");
    fprintf(stream, "                         Run built-in read-only stateful update sample\n");
    fprintf(stream, "      --telegram-read-once-state <file> <offset-file>\n");
    fprintf(stream, "                         Read pending updates and save a persistent offset\n");
    fprintf(stream, "      --telegram-read-once-state-default <offset-file>\n");
    fprintf(stream, "                         Stateful read pending updates with default token file\n");
    fprintf(stream, "      --telegram-read-loop <file> <offset-file> <poll-seconds> <max-iterations>\n");
    fprintf(stream, "                         Run bounded stateful read polling\n");
    fprintf(stream, "      --telegram-read-loop-default <offset-file> <poll-seconds> <max-iterations>\n");
    fprintf(stream, "                         Run bounded read polling with default token file\n");
    fprintf(stream, "      --telegram-inbox-self-test\n");
    fprintf(stream, "                         Run built-in inbox-format update sample\n");
    fprintf(stream, "      --telegram-inbox <file> <offset-file>\n");
    fprintf(stream, "                         Print pending updates in inbox format and save offset\n");
    fprintf(stream, "      --telegram-inbox-default <offset-file>\n");
    fprintf(stream, "                         Inbox read using the default token file\n");
    fprintf(stream, "      --telegram-inbox-loop <file> <offset-file> <poll-seconds> <max-iterations>\n");
    fprintf(stream, "                         Run bounded inbox polling\n");
    fprintf(stream, "      --telegram-inbox-loop-default <offset-file> <poll-seconds> <max-iterations>\n");
    fprintf(stream, "                         Run bounded inbox polling with default token file\n");
    fprintf(stream, "      --telegram-echo-once-self-test\n");
    fprintf(stream, "                         Run built-in one-shot echo flow sample\n");
    fprintf(stream, "      --telegram-echo-once <file> [offset]\n");
    fprintf(stream, "                         Read one update and echo its text back\n");
    fprintf(stream, "      --telegram-echo-once-default [offset]\n");
    fprintf(stream, "                         Echo one update using the default token file\n");
    fprintf(stream, "      --telegram-echo-once-state <file> <offset-file>\n");
    fprintf(stream, "                         Echo pending updates using a persistent offset file\n");
    fprintf(stream, "      --telegram-echo-once-state-default <offset-file>\n");
    fprintf(stream, "                         Stateful echo pending updates with default token file\n");
    fprintf(stream, "      --telegram-echo-loop <file> <offset-file> <poll-seconds> <max-iterations>\n");
    fprintf(stream, "                         Run bounded stateful echo polling\n");
    fprintf(stream, "      --telegram-echo-loop-default <offset-file> <poll-seconds> <max-iterations>\n");
    fprintf(stream, "                         Run bounded echo polling with default token file\n");
    fprintf(stream, "      --telegram-send-message-self-test\n");
    fprintf(stream, "                         Run built-in Bot API sendMessage parser sample\n");
    fprintf(stream, "      --telegram-send-message <file> <chat-id> <text>\n");
    fprintf(stream, "                         Send a Telegram message with token loaded from file\n");
    fprintf(stream, "      --telegram-send-message-default <chat-id> <text>\n");
    fprintf(stream, "                         Send a Telegram message with default token file\n");
    fprintf(stream, "      --telegram-send <file> <chat-id> <text>\n");
    fprintf(stream, "                         Alias for --telegram-send-message\n");
    fprintf(stream, "      --telegram-send-default <chat-id> <text>\n");
    fprintf(stream, "                         Alias for --telegram-send-message-default\n");
}
