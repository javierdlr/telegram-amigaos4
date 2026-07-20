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


/* Scan the WBStartup arg names for "TUI" (case-insensitive). Amiga-only; the
   host build has no Workbench and returns 0. Defensive: any null -> GUI. */
#if defined(__amigaos3__) || defined(__amigaos4__) || defined(__MORPHOS__) || \
    defined(__AROS__)
#include <workbench/startup.h>
static int tg_main_wb_wants_tui(char **argv)
{
    struct WBStartup *wb = (struct WBStartup *)argv;
    long i;

    if (wb == 0 || wb->sm_ArgList == 0) {
        return 0;
    }
    for (i = 0; i < wb->sm_NumArgs; ++i) {
        const char *n = (const char *)wb->sm_ArgList[i].wa_Name;
        const char *p;

        for (p = n; p != 0 && *p != '\0'; ++p) {
            if ((p[0] == 'T' || p[0] == 't') &&
                (p[1] == 'U' || p[1] == 'u') &&
                (p[2] == 'I' || p[2] == 'i')) {
                return 1;
            }
        }
    }
    return 0;
}
#else
static int tg_main_wb_wants_tui(char **argv)
{
    (void)argv;
    return 0;
}
#endif

static int tg_main_finish(int result)
{
    tg_platform_shutdown();
    return result;
}

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
        /* Workbench: pick GUI vs TUI from the launch icon's name. A tool icon
           for the binary launches the GUI; the TUI ships as a project icon
           named "...-TUI" whose DefaultTool is this binary, so scanning the
           WBStartup arg names for "TUI" tells the two apart -- no wrapper
           scripts (papiosaur / Easy2Install suggestion). */
        int want_tui = tg_main_wb_wants_tui(argv);

        if (want_tui) {
            static char *tui_argv[6];

            tg_platform_workbench_init();
            if (!tg_platform_workbench_tui_console()) {
                return tg_main_finish(0); /* no console possible */
            }
            /* One line of drop status in the console scrollback: when a field
               report says "drag-and-drop does nothing", this says WHY. */
            printf("[file drag-and-drop: %s]\n",
                   tg_platform_console_drop_diag());
            tui_argv[0] = "TelegramAmiga";
            tui_argv[1] = "--mtproto-start-file";
            tui_argv[2] = "data/telegram-api.txt";
            tui_argv[3] = "telegram-auth.bin";
            tui_argv[4] = "data/phone-code-hash.txt";
            tui_argv[5] = "data/telegram-peers.txt";
            {
                int rc = tg_app_run(6, tui_argv);

                /* Farewell hint on the WAIT console: the window stays so the
                   last output remains readable, and (when quit came from the
                   close gadget) that first click was consumed as the quit
                   event -- tell the user one more click dismisses it. */
                printf("\n--- Telegram Amiga closed. "
                       "Click the window's close gadget to dismiss. ---\n");
                fflush(stdout);
                /* Give the CON: handle back, or the window can never die:
                   the close gadget only works once every handle is gone. */
                tg_platform_workbench_tui_console_close();
                return tg_main_finish(rc);
            }
        } else {
            static char *wb_argv[3];
            FILE *redir;
            /* GUI: no console -- redirect BEFORE any output so the lazy console
               window is never opened. */
            redir = freopen("NIL:", "w", stdout);
            (void)redir;
            redir = freopen("NIL:", "w", stderr);
            (void)redir;
            tg_platform_workbench_init();
            wb_argv[0] = "TelegramAmiga";
            wb_argv[1] = "--gui-live";
            wb_argv[2] = "data/telegram-peers.txt";
            return tg_main_finish(tg_app_run(3, wb_argv));
        }
    }
    return tg_main_finish(tg_app_run(argc, argv));
}
