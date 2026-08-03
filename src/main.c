/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "tg_app.h"
#include "tg_gui_session.h" /* tg_gui_log: diagnostic trace */
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

/* AmigaDOS scans this application-level cookie before entering main(). Keep it
   in the shared entry module so OS3, OS4 and both AROS binaries cannot drift.
   It is inert in the host test binary, where CI can still prove it survived
   linking. MorphOS additionally needs its PPC-specific __stack declaration. */
#if defined(__GNUC__)
__attribute__((used))
#endif
static const char tg_amiga_stack_cookie[] = TG_PLATFORM_SAFE_STACK_COOKIE;


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
#ifdef TG_DIAG_TRACE
    tg_gui_log("diag: platform shutdown");
#endif
    tg_platform_shutdown();
#ifdef TG_DIAG_TRACE
    /* Last probe we control: whatever happens after this line happens in
       the C runtime's own exit path (stdio close, library close, WBStartup
       reply, UnLoadSeg). */
    tg_gui_log("diag: shutdown done, returning to runtime exit");
#endif
    return result;
}

static int tg_main_body(int argc, char **argv)
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
        /* Prefer an explicit TUI_MODE tooltype on the launched icon (issue #9,
           javierdlr); fall back to the filename heuristic so the default
           byte-identical icons still work with no tooltype at all. */
        int tt;
        int want_tui;

#ifdef TG_DIAG_TRACE
        /* Diagnostic build (experimental 68000 lane): the trail starts at the
           very first instruction of main, so a field report can tell "never
           reached main" from "died at step N". */
        tg_gui_log_enable();
        tg_gui_log("diag: main, workbench launch");
#endif
        tt = tg_platform_wb_tui_mode(argv);
        want_tui = (tt >= 0) ? tt : tg_main_wb_wants_tui(argv);
#ifdef TG_DIAG_TRACE
        tg_gui_log(want_tui ? "diag: launching TUI" : "diag: launching GUI");
#endif

        if (want_tui) {
            static char *tui_argv[6];

#ifdef TG_DIAG_TRACE
            tg_gui_log("diag: workbench_init");
#endif
            tg_platform_workbench_init();
#ifdef TG_DIAG_TRACE
            tg_gui_log("diag: opening TUI console");
#endif
            if (!tg_platform_workbench_tui_console()) {
#ifdef TG_DIAG_TRACE
                tg_gui_log("diag: NO CONSOLE, giving up");
#endif
                return tg_main_finish(0); /* no console possible */
            }
#ifdef TG_DIAG_TRACE
            tg_gui_log("diag: console ready, stdio rebound");
#endif
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
                int rc;

#ifdef TG_DIAG_TRACE
                tg_gui_log("diag: entering tg_app_run (TUI)");
#endif
                rc = tg_app_run(6, tui_argv);
#ifdef TG_DIAG_TRACE
                tg_gui_log("diag: tg_app_run returned");
#endif

                /* Farewell hint on the WAIT console: the window stays so the
                   last output remains readable, and (when quit came from the
                   close gadget) that first click was consumed as the quit
                   event -- tell the user one more click dismisses it. */
                printf("\n--- Telegram Amiga closed. "
                       "Click the window's close gadget to dismiss. ---\n");
                fflush(stdout);
#ifdef TG_DIAG_TRACE
                tg_gui_log("diag: farewell printed");
#endif
                /* Give the CON: handle back, or the window can never die:
                   the close gadget only works once every handle is gone. */
                tg_platform_workbench_tui_console_close();
#ifdef TG_DIAG_TRACE
                tg_gui_log("diag: console closed");
#endif
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

int main(int argc, char **argv)
{
    return tg_platform_run_with_safe_stack(tg_main_body, argc, argv);
}
