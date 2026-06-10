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
 * Colour choices stay within pens 1-3 plus bold/italic/inverse attributes so
 * the scheme degrades gracefully on a default 4-colour AmigaOS 3.x shell
 * screen; 8-colour shells and ANSI terminals simply render the same pens with
 * richer palettes.
 */

#include "tg_console_ui.h"

static int tg_ui_color_mode = TG_UI_COLOR_AUTO;
static int tg_ui_interactive = 0;
static int tg_ui_charset = TG_UI_CHARSET_LATIN1;

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

static const char *tg_console_ui_role_sequence(int role)
{
    switch (role) {
    case TG_UI_ROLE_PEER:
        return "\033[1;33m"; /* bold + pen 3 */
    case TG_UI_ROLE_OWN:
        return "\033[32m"; /* pen 2 */
    case TG_UI_ROLE_SYSTEM:
        return "\033[3m"; /* italic */
    case TG_UI_ROLE_NOTIFY:
        return "\033[7m"; /* inverse video */
    case TG_UI_ROLE_PROMPT:
        return "\033[1m"; /* bold */
    case TG_UI_ROLE_MARKER:
        return "\033[1;32m"; /* bold + pen 2 */
    case TG_UI_ROLE_GROUP:
        return "\033[3m"; /* italic, reads as context */
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
