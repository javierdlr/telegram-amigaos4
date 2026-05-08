/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <stdio.h>
#include <string.h>

#include "tg_app.h"
#include "tg_config.h"
#include "tg_log.h"
#include "tg_net.h"
#include "tg_platform.h"

static int tg_run_http_test(const tg_config *config)
{
    tg_net_connection connection;
    tg_net_status net_status;
    unsigned long bytes_done;
    char net_error[128];
    char request[512];
    char response[512];
    unsigned long request_len;
    unsigned long response_len;

    if (strlen(config->http_test_host) + strlen(config->http_test_path) + 64 >= sizeof(request)) {
        puts("http test: request too large");
        return 1;
    }

    strcpy(request, "GET ");
    strcat(request, config->http_test_path);
    strcat(request, " HTTP/1.0\r\nHost: ");
    strcat(request, config->http_test_host);
    strcat(request, "\r\nConnection: close\r\n\r\n");
    request_len = (unsigned long)strlen(request);

    net_error[0] = '\0';
    net_status = tg_net_connect(&connection, config->http_test_host, config->http_test_port,
                                net_error, sizeof(net_error));
    if (net_status != TG_NET_OK) {
        printf("http test: connect failed: %s", tg_net_status_name(net_status));
        if (net_error[0] != '\0') {
            printf(" (%s)", net_error);
        }
        printf("\n");
        return 2;
    }

    net_status = tg_net_send(&connection, request, request_len, &bytes_done,
                             net_error, sizeof(net_error));
    if (net_status != TG_NET_OK) {
        printf("http test: send failed: %s", tg_net_status_name(net_status));
        if (net_error[0] != '\0') {
            printf(" (%s)", net_error);
        }
        printf("\n");
        tg_net_close(&connection);
        return 2;
    }

    net_status = tg_net_recv(&connection, response, sizeof(response) - 1, &response_len,
                             net_error, sizeof(net_error));
    tg_net_close(&connection);
    if (net_status != TG_NET_OK) {
        printf("http test: recv failed: %s", tg_net_status_name(net_status));
        if (net_error[0] != '\0') {
            printf(" (%s)", net_error);
        }
        printf("\n");
        return 2;
    }

    response[response_len] = '\0';
    printf("http test: %s:%s%s ok, received %lu bytes\n",
           config->http_test_host, config->http_test_port,
           config->http_test_path, response_len);
    printf("%s\n", response);
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

    return 0;
}
