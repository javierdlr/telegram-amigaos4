/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include "tg_app.h"
#include "tg_platform.h"

int main(int argc, char **argv)
{
    /*
     * Workbench launch (icon double-click): the Amiga C runtimes (clib2 on
     * m68k/OS4, libnix on MorphOS, AROS) call main() with argc == 0 and the
     * WBStartup message already stored and auto-replied at exit. There is no
     * console, so behave like the GUI launcher script did: CurrentDir to
     * PROGDIR: and run the live GUI against the peer cache next to the binary.
     * A flashless replacement for the IconX launcher. CLI use is untouched
     * (argc >= 1 always parses normally), so this is fail-safe: if a runtime
     * ever does not zero argc, the feature simply does not activate.
     */
    if (argc == 0) {
        static char *wb_argv[3];
        tg_platform_workbench_init();
        wb_argv[0] = "telegram-test";
        wb_argv[1] = "--gui-live";
        wb_argv[2] = "telegram-peers.txt";
        return tg_app_run(3, wb_argv);
    }
    return tg_app_run(argc, argv);
}
