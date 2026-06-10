/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Console UI role colours. See tg_console_ui.h for the model.
 *
 * Escape sequences use the two-byte CSI introducer ESC '[' rather than the
 * single byte 0x9B: Amiga console.device and its MorphOS/AROS/OS4 descendants
 * accept both, and ESC '[' also works on every ANSI terminal reached over
 * ssh/serial, while raw 0x9B would collide with UTF-8 there.
 *
 * Pen assumptions (stock Workbench-family palettes, verified on real
 * hardware): pen 0 = window grey, pen 1 = black, pen 2 = white, pen 3 =
 * blue/accent. The DARK theme paints the chat white-on-black (bg pen 1) with
 * bold/inverse accents; italics are never used because Amiga consoles render
 * them as sheared glyphs that are hard to read on real screens. On ANSI
 * terminals the same sequences map to black bg / white-ish fg, so one scheme
 * serves every target.
 */

#include "tg_console_ui.h"

static int tg_ui_color_mode = TG_UI_COLOR_AUTO;
static int tg_ui_interactive = 0;
static int tg_ui_charset = TG_UI_CHARSET_LATIN1;
static int tg_ui_theme = TG_UI_THEME_DARK;
static int tg_ui_screen_entered = 0;

void tg_console_ui_set_color_mode(int mode)
{
    if (mode == TG_UI_COLOR_OFF || mode == TG_UI_COLOR_ON ||
        mode == TG_UI_COLOR_AUTO) {
        tg_ui_color_mode = mode;
    }
}

int tg_console_ui_color_mode(void)
{
    return tg_ui_color_mode;
}

void tg_console_ui_set_interactive(int interactive)
{
    tg_ui_interactive = interactive ? 1 : 0;
}

int tg_console_ui_color_active(void)
{
    if (tg_ui_color_mode == TG_UI_COLOR_ON) {
        return 1;
    }
    if (tg_ui_color_mode == TG_UI_COLOR_AUTO) {
        return tg_ui_interactive;
    }
    return 0;
}

void tg_console_ui_set_theme(int theme)
{
    if (theme == TG_UI_THEME_DARK || theme == TG_UI_THEME_PLAIN) {
        tg_ui_theme = theme;
    }
}

int tg_console_ui_theme(void)
{
    return tg_ui_theme;
}

/* Every sequence starts from SGR 0 so segments cannot leak attributes into
   each other; the dark variants re-apply the black background each time so
   the row stays uniform. */
static const char *tg_console_ui_role_sequence(int role)
{
    if (tg_ui_theme == TG_UI_THEME_DARK) {
        switch (role) {
        case TG_UI_ROLE_PEER:
            return "\033[0;41;1;33m"; /* bold accent on black */
        case TG_UI_ROLE_OWN:
            return "\033[0;41;1;32m"; /* bold white on black */
        case TG_UI_ROLE_SYSTEM:
            return "\033[0;41;30m"; /* window-grey on black: quiet */
        case TG_UI_ROLE_NOTIFY:
            return "\033[0;7m"; /* inverse block: pops */
        case TG_UI_ROLE_PROMPT:
            return "\033[0;41;1;32m"; /* bold white on black */
        case TG_UI_ROLE_MARKER:
            return "\033[0;41;1;33m"; /* bold accent on black */
        case TG_UI_ROLE_GROUP:
            return "\033[0;41;30m"; /* quiet context prefix */
        default:
            return "\033[0;41;32m"; /* base: white on black */
        }
    }
    switch (role) {
    case TG_UI_ROLE_PEER:
        return "\033[0;1;33m"; /* bold + pen 3 */
    case TG_UI_ROLE_OWN:
        return "\033[0;1m"; /* bold */
    case TG_UI_ROLE_SYSTEM:
        return "\033[0;33m"; /* pen 3, no italics */
    case TG_UI_ROLE_NOTIFY:
        return "\033[0;7m"; /* inverse video */
    case TG_UI_ROLE_PROMPT:
        return "\033[0;1m"; /* bold */
    case TG_UI_ROLE_MARKER:
        return "\033[0;1;33m"; /* bold + pen 3 */
    case TG_UI_ROLE_GROUP:
        return "\033[0;33m"; /* pen 3 context prefix */
    default:
        return "\033[0m";
    }
}

void tg_console_ui_role(FILE *stream, int role)
{
    if (stream == 0 || !tg_console_ui_color_active()) {
        return;
    }
    fputs(tg_console_ui_role_sequence(role), stream);
}

void tg_console_ui_reset(FILE *stream)
{
    tg_console_ui_role(stream, TG_UI_ROLE_RESET);
}

void tg_console_ui_set_charset(int charset)
{
    if (charset == TG_UI_CHARSET_LATIN1 || charset == TG_UI_CHARSET_UTF8) {
        tg_ui_charset = charset;
    }
}

int tg_console_ui_charset(void)
{
    return tg_ui_charset;
}

void tg_console_ui_enter_screen(FILE *stream)
{
    if (stream == 0 || !tg_console_ui_color_active() ||
        tg_ui_theme != TG_UI_THEME_DARK) {
        return;
    }
    /* Base attributes, then erase the whole window so it adopts the black
       background; scrolled-in lines keep it from there on. */
    fputs(tg_console_ui_role_sequence(TG_UI_ROLE_RESET), stream);
    fputs("\033[H\033[J", stream);
    fflush(stream);
    tg_ui_screen_entered = 1;
}

void tg_console_ui_leave_screen(FILE *stream)
{
    if (stream == 0 || !tg_ui_screen_entered) {
        return;
    }
    fputs("\033[0m\n", stream);
    fflush(stream);
    tg_ui_screen_entered = 0;
}
