/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

int main(void)
{
    static const char msg[] = "telegram-amiga mincrt write reached\n";
    Write(Output(), (APTR)msg, (LONG)(sizeof(msg) - 1));
    return 0;
}
