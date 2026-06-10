/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Full-screen console TUI. See tg_console_tui.h for the model and the
 * console sequences it relies on.
 */

#include <string.h>

#include "tg_console_tui.h"
#include "tg_console_ui.h"
#include "tg_platform.h"

static int tg_tui_active = 0;
static unsigned int tg_tui_rows = 0U;
static unsigned int tg_tui_columns = 0U;

/* First and last transcript rows; status is row 1, input is tg_tui_rows. */
#define TG_TUI_REGION_TOP 2U
#define TG_TUI_REGION_BOTTOM (tg_tui_rows - 2U)
#define TG_TUI_MIN_ROWS 8U
#define TG_TUI_MIN_COLUMNS 20U

static void tg_tui_goto(FILE *stream, unsigned int row, unsigned int column)
{
    fprintf(stream, TG_UI_CSI "%u;%uH", row, column);
}

int tg_console_tui_query_size(FILE *stream,
                              unsigned int *rows,
                              unsigned int *columns)
{
    char ch;
    unsigned long values[8];
    unsigned long value_count;
    unsigned long current;
    int have_digit;
    int guard;

    if (stream == 0 || rows == 0 || columns == 0) {
        return 0;
    }
    /* WINDOW STATUS REQUEST: the console answers on the input stream with
       CSI 1;1;<rows>;<cols> SP r. */
    fputs(TG_UI_CSI "0 q", stream);
    fflush(stream);

    /* Wait for a CSI introducer (tolerate pending unrelated bytes). */
    guard = 0;
    for (;;) {
        if (tg_platform_stdin_read_char(2UL, &ch) <= 0) {
            return 0;
        }
        if (ch == (char)0x9b || ch == (char)0x1b) {
            if (ch == (char)0x1b) {
                if (tg_platform_stdin_read_char(1UL, &ch) <= 0 || ch != '[') {
                    continue;
                }
            }
            break;
        }
        if (++guard > 64) {
            return 0;
        }
    }
    value_count = 0UL;
    current = 0UL;
    have_digit = 0;
    for (guard = 0; guard < 32; ++guard) {
        if (tg_platform_stdin_read_char(1UL, &ch) <= 0) {
            return 0;
        }
        if (ch >= '0' && ch <= '9') {
            current = (current * 10UL) + (unsigned long)(ch - '0');
            have_digit = 1;
            continue;
        }
        if (have_digit && value_count < 8UL) {
            values[value_count] = current;
            ++value_count;
        }
        current = 0UL;
        have_digit = 0;
        if (ch == ';' || ch == ' ') {
            continue;
        }
        break; /* final byte (expected 'r') */
    }
    if (ch != 'r' || value_count < 4UL) {
        return 0;
    }
    if (values[2] < TG_TUI_MIN_ROWS || values[3] < TG_TUI_MIN_COLUMNS ||
        values[2] > 200UL || values[3] > 500UL) {
        return 0;
    }
    *rows = (unsigned int)values[2];
    *columns = (unsigned int)values[3];
    return 1;
}

int tg_console_tui_active(void)
{
    return tg_tui_active;
}

/* Writes text clipped to the window width, padding with erase-to-EOL. */
static void tg_tui_clipped_line(FILE *stream, const char *text)
{
    unsigned int printed;

    printed = 0U;
    while (text != 0 && *text != '\0' && printed + 1U < tg_tui_columns) {
        /* Console escape sequences do not consume columns; pass them through
           without counting (introducer to final byte 0x40-0x7e). */
        if (*text == (char)0x9b || (*text == (char)0x1b && text[1] == '[')) {
            fputc(*text, stream);
            if (*text == (char)0x1b) {
                fputc('[', stream);
                ++text;
            }
            ++text;
            while (*text != '\0' &&
                   !((unsigned char)*text >= 0x40U &&
                     (unsigned char)*text <= 0x7eU)) {
                fputc(*text, stream);
                ++text;
            }
            if (*text != '\0') {
                fputc(*text, stream);
                ++text;
            }
            continue;
        }
        fputc(*text, stream);
        ++text;
        ++printed;
    }
    fputs(TG_UI_CSI "K", stream);
}

void tg_console_tui_status(FILE *stream, const char *status_text)
{
    if (stream == 0 || !tg_tui_active) {
        return;
    }
    tg_tui_goto(stream, 1U, 1U);
    tg_console_ui_role(stream, TG_UI_ROLE_NOTIFY);
    tg_tui_clipped_line(stream, status_text != 0 ? status_text : "");
    tg_console_ui_reset(stream);
    fflush(stream);
}

int tg_console_tui_enter(FILE *stream, const char *status_text)
{
    unsigned int rows;
    unsigned int columns;

    if (stream == 0 || tg_tui_active) {
        return tg_tui_active;
    }
    if (!tg_console_tui_query_size(stream, &rows, &columns)) {
        return 0;
    }
    tg_tui_rows = rows;
    tg_tui_columns = columns;
    tg_tui_active = 1;
    /* Base attributes + full clear, then the fixed chrome. */
    tg_console_ui_reset(stream);
    fputs(TG_UI_CSI "H" TG_UI_CSI "J", stream);
    tg_console_tui_status(stream, status_text);
    tg_tui_goto(stream, tg_tui_rows - 1U, 1U);
    tg_console_ui_role(stream, TG_UI_ROLE_SYSTEM);
    tg_tui_clipped_line(stream, "------------------------------------------"
                                "--------------------------------------");
    tg_console_ui_reset(stream);
    tg_tui_goto(stream, tg_tui_rows, 1U);
    fputs(TG_UI_CSI "K", stream);
    fflush(stream);
    return 1;
}

void tg_console_tui_line(FILE *stream, const char *text)
{
    if (stream == 0 || !tg_tui_active) {
        return;
    }
    /* Scroll the transcript region: drop its top row (everything below
       shifts up, input row included), then re-open a blank row just above
       the separator so the chrome returns to its place. */
    tg_tui_goto(stream, TG_TUI_REGION_TOP, 1U);
    fputs(TG_UI_CSI "M", stream);
    tg_tui_goto(stream, TG_TUI_REGION_BOTTOM, 1U);
    fputs(TG_UI_CSI "L", stream);
    tg_tui_clipped_line(stream, text != 0 ? text : "");
    fflush(stream);
}

void tg_console_tui_input(FILE *stream,
                          const char *prompt,
                          const char *pending,
                          unsigned long pending_length)
{
    unsigned int printed;
    unsigned long i;

    if (stream == 0 || !tg_tui_active) {
        return;
    }
    tg_tui_goto(stream, tg_tui_rows, 1U);
    fputs(TG_UI_CSI "K", stream);
    printed = 0U;
    if (prompt != 0) {
        tg_console_ui_role(stream, TG_UI_ROLE_PROMPT);
        while (*prompt != '\0' && printed + 2U < tg_tui_columns) {
            fputc(*prompt, stream);
            ++prompt;
            ++printed;
        }
        tg_console_ui_reset(stream);
    }
    if (pending != 0) {
        /* Show the tail when the pending text outgrows the window. */
        i = 0UL;
        if (pending_length + (unsigned long)printed + 2UL >
            (unsigned long)tg_tui_columns) {
            i = pending_length + (unsigned long)printed + 2UL -
                (unsigned long)tg_tui_columns;
        }
        for (; i < pending_length && printed + 2U < tg_tui_columns; ++i) {
            fputc(pending[i], stream);
            ++printed;
        }
    }
    fflush(stream);
}

void tg_console_tui_leave(FILE *stream)
{
    if (stream == 0 || !tg_tui_active) {
        return;
    }
    tg_tui_active = 0;
    tg_tui_goto(stream, tg_tui_rows, 1U);
    fputs(TG_UI_CSI "0m\n", stream);
    fflush(stream);
}

int tg_console_tui_self_test(FILE *stream)
{
    char line[64];
    char ch;
    unsigned int i;
    unsigned int rows;
    unsigned int columns;
    int raw_ok;

    if (stream == 0) {
        return 2;
    }
    raw_ok = tg_platform_stdin_set_raw(1) == 0;
    if (!raw_ok) {
        fputs("tui-test: raw console mode unavailable\n", stream);
        return 2;
    }
    tg_console_ui_set_interactive(1);
    if (!tg_console_tui_query_size(stream, &rows, &columns)) {
        tg_platform_stdin_set_raw(0);
        fputs("tui-test: window size query failed\n", stream);
        return 2;
    }
    fprintf(stream, "tui-test: window %ux%u, entering in 2s...\n", columns,
            rows);
    fflush(stream);
    (void)tg_platform_stdin_read_char(2UL, &ch);
    if (!tg_console_tui_enter(stream,
                              " Telegram Amiga -- TUI test (q quits) ")) {
        tg_platform_stdin_set_raw(0);
        fputs("tui-test: enter failed\n", stream);
        return 2;
    }
    for (i = 1U; i <= 30U; ++i) {
        sprintf(line, "transcript line %u of 30", i);
        tg_console_tui_line(stream, line);
    }
    tg_console_tui_input(stream, "you: ", "type q to quit", 14UL);
    for (;;) {
        if (tg_platform_stdin_read_char(60UL, &ch) <= 0) {
            break;
        }
        if (ch == 'q' || ch == 'Q' || ch == 3) {
            break;
        }
        sprintf(line, "key 0x%02x", (unsigned char)ch);
        tg_console_tui_line(stream, line);
        tg_console_tui_input(stream, "you: ", "type q to quit", 14UL);
    }
    tg_console_tui_leave(stream);
    tg_platform_stdin_set_raw(0);
    fputs("tui-test: done\n", stream);
    return 0;
}
