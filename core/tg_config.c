/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <string.h>

#include "tg_config.h"
#include "tg_platform.h"

void tg_config_init(tg_config *config)
{
    config->data_dir = tg_platform_default_data_dir();
    config->log_level = TG_LOG_INFO;
    config->show_help = 0;
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
}
