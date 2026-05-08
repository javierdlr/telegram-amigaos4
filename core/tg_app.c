/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <stdio.h>

#include "tg_app.h"
#include "tg_platform.h"

int tg_app_run(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    puts("telegram-amiga bootstrap");
    printf("platform: %s\n", tg_platform_name());
    tg_platform_log("core initialized");

    return 0;
}
