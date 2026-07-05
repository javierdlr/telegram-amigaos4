/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "tg_app.h"
#include "tg_version.h"

/* Amiga $VER version tag (GitHub issue #3): lets the shell "Version" command
   report the binary's version on every Amiga-like OS. Must be a real string
   in the data section -- "used" keeps it alive if the linker ever GCs. */
#if defined(__GNUC__)
__attribute__((used))
#endif
static const char tg_amiga_ver_tag[] =
    "$VER: TelegramAmiga " TG_VERSION " (" TG_VERSION_DATE ")";
#include "tg_platform.h"

int main(int argc, char **argv)
{
    /*
     * Workbench launch (icon double-click): the Amiga C runtimes (clib2 on
     * m68k/OS4, libnix on MorphOS, AROS) call main() with argc == 0 and the
     * WBStartup message already stored and auto-replied at exit. There is no
     * console, so behave like the GUI launcher script did: send stdout/stderr
     * to NIL: (otherwise the runtime opens a console window for the binary's
     * log lines -- exactly what the IconX launcher avoided with "Run >NIL:"),
     * CurrentDir to PROGDIR:, and run the live GUI against the peer cache next
     * to the binary. CLI use is untouched (argc >= 1 always parses normally),
     * so this is fail-safe: if a runtime ever does not zero argc, the feature
     * simply does not activate.
     */
    if (argc == 0) {
        static char *wb_argv[3];
        FILE *redir;
        /* Redirect BEFORE any output so the lazy console is never opened. */
        redir = freopen("NIL:", "w", stdout);
        (void)redir;
        redir = freopen("NIL:", "w", stderr);
        (void)redir;
        tg_platform_workbench_init();
        wb_argv[0] = "TelegramAmiga";
        wb_argv[1] = "--gui-live";
        wb_argv[2] = "telegram-peers.txt";
        return tg_app_run(3, wb_argv);
    }
    return tg_app_run(argc, argv);
}
