/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "tg_app.h"
#include "tg_config.h"
#include "tg_https.h"
#include "tg_http.h"
#include "tg_json.h"
#include "tg_log.h"
#include "tg_net.h"
#include "tg_platform.h"
#include "tg_telegram.h"

static int tg_run_http_test(const tg_config *config)
{
    tg_http_status http_status;
    tg_http_parse_status parse_status;
    tg_http_response parsed_response;
    tg_net_status net_status;
    char net_error[128];
    char response[2048];
    unsigned long response_len;

    http_status = tg_http_get(config->http_test_host, config->http_test_port,
                              config->http_test_path, response, sizeof(response),
                              &response_len, &net_status, net_error, sizeof(net_error));
    if (http_status != TG_HTTP_OK) {
        printf("http test: failed: %s", tg_http_status_name(http_status));
        if (http_status == TG_HTTP_NET_ERROR) {
            printf(" / %s", tg_net_status_name(net_status));
        }
        if (net_error[0] != '\0') {
            printf(" (%s)", net_error);
        }
        printf("\n");
        return 2;
    }

    printf("http test: %s:%s%s ok, received %lu bytes\n",
           config->http_test_host, config->http_test_port,
           config->http_test_path, response_len);

    parse_status = tg_http_parse_response(response, response_len, &parsed_response);
    if (parse_status != TG_HTTP_PARSE_OK) {
        printf("http parse: failed: %s\n", tg_http_parse_status_name(parse_status));
        printf("%s\n", response);
        return 0;
    }

    printf("http status: %d", parsed_response.status_code);
    if (parsed_response.reason_length > 0) {
        printf(" %.*s", (int)parsed_response.reason_length, parsed_response.reason);
    }
    printf("\n");
    printf("http body: %lu bytes\n", parsed_response.body_length);
    printf("%.*s\n", (int)parsed_response.body_length, parsed_response.body);
    return 0;
}

static int tg_run_https_test(const tg_config *config)
{
    tg_https_status https_status;
    tg_http_parse_status parse_status;
    tg_http_response parsed_response;
    tg_tls_status tls_status;
    tg_net_status net_status;
    char net_error[128];
    char response[2048];
    unsigned long response_len;

    https_status = tg_https_get(config->https_test_host, config->https_test_port,
                                config->https_test_path, response, sizeof(response),
                                &response_len, &tls_status, &net_status,
                                net_error, sizeof(net_error));
    if (https_status != TG_HTTPS_OK) {
        printf("https test: failed: %s", tg_https_status_name(https_status));
        if (https_status == TG_HTTPS_TLS_ERROR) {
            printf(" / %s", tg_tls_status_name(tls_status));
            if (tls_status == TG_TLS_NET_ERROR) {
                printf(" / %s", tg_net_status_name(net_status));
            }
        }
        if (net_error[0] != '\0') {
            printf(" (%s)", net_error);
        }
        printf("\n");
        return 2;
    }

    printf("https test: %s:%s%s ok, received %lu bytes\n",
           config->https_test_host, config->https_test_port,
           config->https_test_path, response_len);

    parse_status = tg_http_parse_response(response, response_len, &parsed_response);
    if (parse_status != TG_HTTP_PARSE_OK) {
        printf("https parse: failed: %s\n", tg_http_parse_status_name(parse_status));
        printf("%s\n", response);
        return 0;
    }

    printf("https status: %d", parsed_response.status_code);
    if (parsed_response.reason_length > 0) {
        printf(" %.*s", (int)parsed_response.reason_length, parsed_response.reason);
    }
    printf("\n");
    printf("https body: %lu bytes\n", parsed_response.body_length);
    printf("%.*s\n", (int)parsed_response.body_length, parsed_response.body);
    return 0;
}

static int tg_run_json_test(const tg_config *config)
{
    tg_json_status json_status;
    tg_json_value value;
    unsigned long json_length;

    json_length = (unsigned long)strlen(config->json_test_input);
    json_status = tg_json_object_get(config->json_test_input, json_length,
                                     config->json_test_field, &value);
    if (json_status != TG_JSON_OK) {
        printf("json test: failed: %s\n", tg_json_status_name(json_status));
        return 2;
    }

    printf("json field: %s\n", config->json_test_field);
    printf("json type: %s\n", tg_json_value_type_name(value.type));

    if (value.type == TG_JSON_VALUE_BOOL) {
        printf("json value: %s\n", value.bool_value ? "true" : "false");
    } else if (value.type == TG_JSON_VALUE_STRING ||
               value.type == TG_JSON_VALUE_NUMBER ||
               value.type == TG_JSON_VALUE_OBJECT ||
               value.type == TG_JSON_VALUE_ARRAY) {
        printf("json value: %.*s\n", (int)value.length, value.start);
    } else {
        printf("json value: null\n");
    }

    return 0;
}

static void tg_print_telegram_response(const tg_telegram_response *response)
{
    printf("telegram ok: %s\n", response->ok ? "true" : "false");
    if (response->has_description) {
        printf("telegram description: %.*s\n",
               (int)response->description_length, response->description);
    }
    if (response->has_result) {
        printf("telegram result type: %s\n",
               tg_json_value_type_name(response->result.type));
        if (response->result.type != TG_JSON_VALUE_NULL &&
            response->result.start != 0) {
            printf("telegram result: %.*s\n",
                   (int)response->result.length, response->result.start);
        }
    }
}

static int tg_run_telegram_json_test_text(const char *json)
{
    tg_telegram_status telegram_status;
    tg_telegram_response response;
    tg_json_status json_status;

    telegram_status = tg_telegram_parse_response(json, (unsigned long)strlen(json),
                                                 &response, &json_status);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram json test: failed: %s", tg_telegram_status_name(telegram_status));
        if (telegram_status == TG_TELEGRAM_JSON_ERROR ||
            telegram_status == TG_TELEGRAM_MISSING_OK) {
            printf(" / %s", tg_json_status_name(json_status));
        }
        printf("\n");
        return 2;
    }

    tg_print_telegram_response(&response);
    return 0;
}

static int tg_run_telegram_json_test(const tg_config *config)
{
    return tg_run_telegram_json_test_text(config->telegram_json_test_input);
}

static int tg_run_telegram_json_self_test(void)
{
    static const char ok_sample[] = "{\"ok\":true,\"result\":{\"id\":123,\"is_bot\":true}}";
    static const char error_sample[] = "{\"ok\":false,\"description\":\"Unauthorized\"}";

    puts("telegram json self-test: ok sample");
    if (tg_run_telegram_json_test_text(ok_sample) != 0) {
        return 2;
    }

    puts("telegram json self-test: error sample");
    if (tg_run_telegram_json_test_text(error_sample) != 0) {
        return 2;
    }

    return 0;
}

static int tg_run_telegram_path_test(const tg_config *config)
{
    tg_telegram_status telegram_status;
    char path[256];
    unsigned long path_length;

    telegram_status = tg_telegram_build_bot_path(config->telegram_path_test_token,
                                                 config->telegram_path_test_method,
                                                 path, sizeof(path), &path_length);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram path test: failed: %s\n",
               tg_telegram_status_name(telegram_status));
        return 2;
    }

    printf("telegram host: %s\n", tg_telegram_api_host());
    printf("telegram path: %s\n", path);
    printf("telegram path length: %lu\n", path_length);
    return 0;
}

static int tg_run_telegram_http_test_text(const char *http_response)
{
    tg_telegram_status telegram_status;
    tg_telegram_http_response response;
    tg_http_parse_status http_parse_status;
    tg_json_status json_status;

    telegram_status = tg_telegram_parse_http_response(http_response,
                                                      (unsigned long)strlen(http_response),
                                                      &response, &http_parse_status,
                                                      &json_status);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram http test: failed: %s", tg_telegram_status_name(telegram_status));
        if (telegram_status == TG_TELEGRAM_HTTP_PARSE_ERROR) {
            printf(" / %s", tg_http_parse_status_name(http_parse_status));
        } else if (telegram_status == TG_TELEGRAM_JSON_ERROR ||
                   telegram_status == TG_TELEGRAM_MISSING_OK) {
            printf(" / %s", tg_json_status_name(json_status));
        }
        printf("\n");
        return 2;
    }

    printf("telegram http status: %d\n", response.http_status_code);
    tg_print_telegram_response(&response.api);
    return 0;
}

static int tg_run_telegram_http_self_test(void)
{
    static const char ok_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":{\"id\":123,\"is_bot\":true}}";
    static const char error_response[] =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":false,\"description\":\"Unauthorized\"}";

    puts("telegram http self-test: ok response");
    if (tg_run_telegram_http_test_text(ok_response) != 0) {
        return 2;
    }

    puts("telegram http self-test: error response");
    if (tg_run_telegram_http_test_text(error_response) != 0) {
        return 2;
    }

    return 0;
}

static int tg_run_telegram_token_file_path_test(const tg_config *config)
{
    tg_telegram_status telegram_status;
    tg_file_status file_status;
    char token[128];
    char path[256];
    unsigned long token_length;
    unsigned long path_length;

    telegram_status = tg_telegram_load_token_file(config->telegram_token_file_path,
                                                  token, sizeof(token),
                                                  &token_length, &file_status);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram token file test: failed: %s", tg_telegram_status_name(telegram_status));
        if (telegram_status == TG_TELEGRAM_FILE_ERROR) {
            printf(" / %s", tg_file_status_name(file_status));
        }
        printf("\n");
        return 2;
    }

    telegram_status = tg_telegram_build_bot_path(token,
                                                 config->telegram_token_file_method,
                                                 path, sizeof(path), &path_length);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram token file path test: failed: %s\n",
               tg_telegram_status_name(telegram_status));
        return 2;
    }

    printf("telegram host: %s\n", tg_telegram_api_host());
    printf("telegram method: %s\n", config->telegram_token_file_method);
    printf("telegram token length: %lu\n", token_length);
    printf("telegram path length: %lu\n", path_length);
    printf("telegram path: <redacted>\n");
    return 0;
}

int tg_app_run(int argc, char **argv)
{
    tg_config config;
    tg_net_status net_status;
    char net_error[128];
    const char *program_name;

    program_name = "telegram-test";
    if (argc > 0 && argv[0] != 0) {
        program_name = argv[0];
    }

    tg_config_init(&config);
    if (tg_config_parse(&config, argc, argv) != 0) {
        tg_config_print_usage(stderr, program_name);
        return 1;
    }

    if (config.show_help) {
        tg_config_print_usage(stdout, program_name);
        return 0;
    }

    tg_log_set_level(config.log_level);

    puts("telegram-amiga bootstrap");
    printf("platform: %s\n", tg_platform_name());
    tg_log(TG_LOG_INFO, "core initialized");
    printf("data dir: %s\n", config.data_dir);

    if (config.run_net_test) {
        net_error[0] = '\0';
        net_status = tg_net_tcp_probe(config.net_test_host, config.net_test_port,
                                      net_error, sizeof(net_error));
        if (net_status != TG_NET_OK) {
            printf("net test: %s:%s failed: %s",
                   config.net_test_host, config.net_test_port,
                   tg_net_status_name(net_status));
            if (net_error[0] != '\0') {
                printf(" (%s)", net_error);
            }
            printf("\n");
            return 2;
        }
        printf("net test: %s:%s ok\n", config.net_test_host, config.net_test_port);
    }

    if (config.run_http_test) {
        return tg_run_http_test(&config);
    }

    if (config.run_https_test) {
        return tg_run_https_test(&config);
    }

    if (config.run_json_test) {
        return tg_run_json_test(&config);
    }

    if (config.run_telegram_json_test) {
        return tg_run_telegram_json_test(&config);
    }

    if (config.run_telegram_json_self_test) {
        return tg_run_telegram_json_self_test();
    }

    if (config.run_telegram_path_test) {
        return tg_run_telegram_path_test(&config);
    }

    if (config.run_telegram_http_self_test) {
        return tg_run_telegram_http_self_test();
    }

    if (config.run_telegram_token_file_path_test) {
        return tg_run_telegram_token_file_path_test(&config);
    }

    return 0;
}
