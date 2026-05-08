/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <stdio.h>

#include "tg_app.h"
#include "tg_config.h"
#include "tg_log.h"
#include "tg_net.h"
#include "tg_platform.h"

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

    return 0;
}
