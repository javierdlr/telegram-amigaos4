/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <stdio.h>

#include "tg_platform.h"

const char *tg_platform_name(void)
{
    return "AmigaOS 4";
}

const char *tg_platform_default_data_dir(void)
{
    return "PROGDIR:";
}

void tg_platform_log(const char *level, const char *message)
{
    printf("[amigaos4:%s] %s\n", level, message);
}
