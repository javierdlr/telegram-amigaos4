/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <stdio.h>

#include "tg_platform.h"

const char *tg_platform_name(void)
{
    return "AmigaOS 3.x";
}

void tg_platform_log(const char *message)
{
    printf("[amigaos3] %s\n", message);
}
