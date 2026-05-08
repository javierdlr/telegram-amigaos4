/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#ifndef TG_CONFIG_H
#define TG_CONFIG_H

#include <stdio.h>

#include "tg_log.h"

typedef struct tg_config {
    const char *data_dir;
    const char *net_test_host;
    const char *net_test_port;
    const char *http_test_host;
    const char *http_test_port;
    const char *http_test_path;
    tg_log_level log_level;
    int show_help;
    int run_net_test;
    int run_http_test;
} tg_config;

void tg_config_init(tg_config *config);
int tg_config_parse(tg_config *config, int argc, char **argv);
void tg_config_print_usage(FILE *stream, const char *program_name);

#endif
