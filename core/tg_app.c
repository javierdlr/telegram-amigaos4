/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <stdio.h>

#include "tg_app.h"
#include "tg_config.h"
#include "tg_log.h"
#include "tg_platform.h"

int tg_app_run(int argc, char **argv)
{
    tg_config config;
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

    return 0;
}
