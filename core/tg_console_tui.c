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
static int tg_tui_enabled = 1;
static int tg_tui_resize_flag = 0;
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
       CSI 1;1;<rows>;<cols> SP r. The input may also hold queued NEWSIZE
       event reports (CSI 12;...|), especially mid-drag: skip those and keep
       parsing CSI sequences until the 'r' answer shows up. */
    fputs(TG_UI_CSI "0 q", stream);
    fflush(stream);

    for (guard = 0; guard < 16; ++guard) {
        int introducer_tries;
        int byte_tries;

        /* Wait for a CSI introducer (tolerate pending unrelated bytes). */
        introducer_tries = 0;
        for (;;) {
            if (tg_platform_stdin_read_char(2UL, &ch) <= 0) {
                return 0;
            }
            if (ch == (char)0x9b || ch == (char)0x1b) {
                if (ch == (char)0x1b) {
                    if (tg_platform_stdin_read_char(1UL, &ch) <= 0 ||
                        ch != '[') {
                        continue;
                    }
                }
                break;
            }
            if (++introducer_tries > 64) {
                return 0;
            }
        }
        value_count = 0UL;
        current = 0UL;
        have_digit = 0;
        for (byte_tries = 0; byte_tries < 32; ++byte_tries) {
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
            break; /* final byte */
        }
        if (ch != 'r') {
            /* Another report (e.g. a queued NEWSIZE event, final '|'):
               ignore it and wait for the real answer. */
            continue;
        }
        if (value_count < 4UL || values[2] < TG_TUI_MIN_ROWS ||
            values[3] < TG_TUI_MIN_COLUMNS || values[2] > 300UL ||
            values[3] > 1000UL) {
            return 0;
        }
        *rows = (unsigned int)values[2];
        *columns = (unsigned int)values[3];
        return 1;
    }
    return 0;
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
        if ((unsigned char)*text < 0x20U) {
            /* Control bytes inside transcript content (stray BEL, CR...)
               would render as odd glyphs on some consoles: drop them. */
            ++text;
            continue;
        }
        fputc(*text, stream);
        ++text;
        ++printed;
    }
    if (tg_console_ui_color_active() &&
        tg_console_ui_theme() == TG_UI_THEME_DARK) {
        /* Paint the rest of the row with real spaces: consoles without
           back-colour-erase (AmigaOS 3.x) ignore the SGR background on
           CSI K, but spaces are characters and fill their cells. */
        while (printed + 1U < tg_tui_columns) {
            fputc(' ', stream);
            ++printed;
        }
        return;
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

void tg_console_tui_set_enabled(int enabled)
{
    tg_tui_enabled = enabled ? 1 : 0;
}

/*
 * Transcript backlog: the most recent lines, kept so a repaint (resize,
 * /add round-trip) can replay them into the fresh region instead of
 * starting blank. Lines are stored as handed to tg_console_tui_line --
 * colour sequences included, pre-clip, so a wider window after a resize
 * shows more of each line, not less.
 */
#define TG_TUI_BACKLOG_LINES 64U
#define TG_TUI_BACKLOG_WIDTH 160U

static char tg_tui_backlog[TG_TUI_BACKLOG_LINES][TG_TUI_BACKLOG_WIDTH];
static unsigned long tg_tui_backlog_total = 0UL;

static void tg_tui_backlog_record(const char *text)
{
    char *slot = tg_tui_backlog[tg_tui_backlog_total % TG_TUI_BACKLOG_LINES];
    unsigned long n = 0UL;

    if (text != 0) {
        while (text[n] != '\0' && n + 1UL < TG_TUI_BACKLOG_WIDTH) {
            slot[n] = text[n];
            ++n;
        }
    }
    slot[n] = '\0';
    ++tg_tui_backlog_total;
}

/* The drawing half of tg_console_tui_line (no recording). */
static void tg_tui_draw_transcript_line(FILE *stream, const char *text)
{
    tg_tui_goto(stream, TG_TUI_REGION_TOP, 1U);
    fputs(TG_UI_CSI "M", stream);
    tg_tui_goto(stream, TG_TUI_REGION_BOTTOM, 1U);
    fputs(TG_UI_CSI "L", stream);
    tg_tui_clipped_line(stream, text != 0 ? text : "");
}

/* Scrollback: how many lines back from "live" the transcript view sits.
   0 = live; while scrolled, new lines only enter the backlog and the view
   stays anchored to its content. */
static unsigned long tg_tui_view_offset = 0UL;

static unsigned long tg_tui_backlog_available(void)
{
    return tg_tui_backlog_total > TG_TUI_BACKLOG_LINES
               ? (unsigned long)TG_TUI_BACKLOG_LINES
               : tg_tui_backlog_total;
}

static unsigned long tg_tui_view_offset_max(void)
{
    unsigned long region;
    unsigned long avail = tg_tui_backlog_available();

    if (tg_tui_rows < 4U) {
        return 0UL;
    }
    region = (unsigned long)tg_tui_rows - 3UL;
    return avail > region ? avail - region : 0UL;
}

/* Redraws the whole transcript region from the backlog at the current view
   offset; the separator doubles as the scrollback indicator. */
static void tg_tui_redraw_region(FILE *stream)
{
    unsigned long region;
    unsigned long avail = tg_tui_backlog_available();
    unsigned long r;

    if (tg_tui_rows < 4U) {
        return;
    }
    region = (unsigned long)tg_tui_rows - 3UL;
    for (r = 0UL; r < region; ++r) {
        unsigned long back = tg_tui_view_offset + (region - 1UL - r);

        tg_tui_goto(stream, TG_TUI_REGION_TOP + (unsigned int)r, 1U);
        fputs(TG_UI_CSI "K", stream);
        if (back < avail) {
            unsigned long index = tg_tui_backlog_total - 1UL - back;
            tg_tui_clipped_line(
                stream, tg_tui_backlog[index % TG_TUI_BACKLOG_LINES]);
        } else {
            tg_tui_clipped_line(stream, "");
        }
    }
    tg_tui_goto(stream, tg_tui_rows - 1U, 1U);
    tg_console_ui_role(stream, TG_UI_ROLE_SYSTEM);
    tg_tui_clipped_line(stream,
                        tg_tui_view_offset > 0UL
                            ? "---- older messages -- Shift+Down: back to "
                              "live ----------------------------------"
                            : "----------------------------------------------"
                              "----------------------------------");
    tg_console_ui_reset(stream);
    fflush(stream);
}

void tg_console_tui_scroll(FILE *stream, int direction)
{
    unsigned long step;
    unsigned long max_offset;

    if (stream == 0 || !tg_tui_active || tg_tui_rows < 4U) {
        return;
    }
    step = ((unsigned long)tg_tui_rows - 3UL) / 2UL;
    if (step == 0UL) {
        step = 1UL;
    }
    max_offset = tg_tui_view_offset_max();
    if (direction > 0) {
        tg_tui_view_offset += step;
        if (tg_tui_view_offset > max_offset) {
            tg_tui_view_offset = max_offset;
        }
    } else {
        tg_tui_view_offset =
            tg_tui_view_offset > step ? tg_tui_view_offset - step : 0UL;
    }
    tg_tui_redraw_region(stream);
}

static void tg_tui_replay_backlog(FILE *stream)
{
    unsigned long region_rows;
    unsigned long count;
    unsigned long start;
    unsigned long i;

    if (tg_tui_rows < 4U) {
        return;
    }
    region_rows = (unsigned long)tg_tui_rows - 3UL;
    count = tg_tui_backlog_total;
    if (count > TG_TUI_BACKLOG_LINES) {
        count = TG_TUI_BACKLOG_LINES;
    }
    if (count > region_rows) {
        count = region_rows;
    }
    start = tg_tui_backlog_total - count;
    for (i = 0UL; i < count; ++i) {
        tg_tui_draw_transcript_line(
            stream, tg_tui_backlog[(start + i) % TG_TUI_BACKLOG_LINES]);
    }
}

static void tg_tui_paint_chrome(FILE *stream, const char *status_text)
{
    /* A chrome repaint (enter, resize, /add round-trip) returns to live. */
    tg_tui_view_offset = 0UL;
    /* Base attributes + full clear, then the fixed chrome. */
    tg_console_ui_reset(stream);
    fputs(TG_UI_CSI "H" TG_UI_CSI "J", stream);
    tg_console_tui_status(stream, status_text);
    tg_tui_goto(stream, tg_tui_rows - 1U, 1U);
    tg_console_ui_role(stream, TG_UI_ROLE_SYSTEM);
    tg_tui_clipped_line(stream, "------------------------------------------"
                                "--------------------------------------");
    tg_console_ui_reset(stream);
    /* What was on screen, back on screen: the repaint should cost the user
       nothing but a blink. */
    tg_tui_replay_backlog(stream);
    tg_tui_goto(stream, tg_tui_rows, 1U);
    fputs(TG_UI_CSI "K", stream);
    fflush(stream);
}

int tg_console_tui_enter(FILE *stream, const char *status_text)
{
    unsigned int rows;
    unsigned int columns;

    if (stream == 0 || tg_tui_active) {
        return tg_tui_active;
    }
    /* The size probe runs even with the full-screen layout disabled: its
       outcome feeds the mini-termcap (an unanswered probe on a raw console
       marks it CSI-deaf, so other writers stop sending sequences it would
       draw as glyphs). */
    if (!tg_console_tui_query_size(stream, &rows, &columns)) {
        tg_console_caps_note_size_query(0);
        return 0;
    }
    tg_console_caps_note_size_query(1);
    if (!tg_tui_enabled) {
        return 0;
    }
    tg_tui_rows = rows;
    tg_tui_columns = columns;
    tg_tui_active = 1;
    tg_tui_resize_flag = 0;
    /* Subscribe to the console's NEWSIZE raw event: resizes then arrive on
       stdin as CSI 12;...| reports, which the line editor turns into a
       pending-resize flag. */
    fputs(TG_UI_CSI "12{", stream);
    tg_tui_paint_chrome(stream, status_text);
    return 1;
}

void tg_console_tui_note_resize(void)
{
    if (tg_tui_active) {
        tg_tui_resize_flag = 1;
    }
}

int tg_console_tui_resize_pending(void)
{
    return tg_tui_resize_flag;
}

int tg_console_tui_resize(FILE *stream, const char *status_text)
{
    unsigned int rows;
    unsigned int columns;
    char drain;
    int drained;

    tg_tui_resize_flag = 0;
    if (stream == 0 || !tg_tui_active) {
        return 0;
    }
    /* A window drag floods stdin with queued NEWSIZE reports; flush them
       before asking for the final geometry. */
    for (drained = 0; drained < 512; ++drained) {
        if (tg_platform_stdin_read_char(0UL, &drain) <= 0) {
            break;
        }
    }
    if (!tg_console_tui_query_size(stream, &rows, &columns)) {
        /* Keep the old geometry rather than tearing the layout down. */
        return 0;
    }
    if (rows == tg_tui_rows && columns == tg_tui_columns) {
        /* Same geometry (a duplicate end-of-drag report, or a console that
           confirms the subscription with an immediate report): repainting
           would wipe the transcript for nothing. */
        return 0;
    }
    tg_tui_rows = rows;
    tg_tui_columns = columns;
    tg_tui_paint_chrome(stream, status_text);
    return 1;
}

void tg_console_tui_line(FILE *stream, const char *text)
{
    if (stream == 0 || !tg_tui_active) {
        return;
    }
    /* Scroll the transcript region: drop its top row (everything below
       shifts up, input row included), then re-open a blank row just above
       the separator so the chrome returns to its place. The line is also
       recorded so resize/re-enter repaints can replay it. */
    tg_tui_backlog_record(text);
    if (tg_tui_view_offset > 0UL) {
        /* Scrolled back: the new line enters the backlog only and the view
           stays anchored to its content (the offset grows with the ring). */
        unsigned long max_offset = tg_tui_view_offset_max();
        if (tg_tui_view_offset < max_offset) {
            ++tg_tui_view_offset;
        }
        return;
    }
    tg_tui_draw_transcript_line(stream, text);
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
        /* The prompt may embed colour role sequences: pass them through
           without counting columns, like the transcript clipper. */
        while (*prompt != '\0' && printed + 2U < tg_tui_columns) {
            if (*prompt == (char)0x9b ||
                (*prompt == (char)0x1b && prompt[1] == '[')) {
                fputc(*prompt, stream);
                if (*prompt == (char)0x1b) {
                    fputc('[', stream);
                    ++prompt;
                }
                ++prompt;
                while (*prompt != '\0' &&
                       !((unsigned char)*prompt >= 0x40U &&
                         (unsigned char)*prompt <= 0x7eU)) {
                    fputc(*prompt, stream);
                    ++prompt;
                }
                if (*prompt != '\0') {
                    fputc(*prompt, stream);
                    ++prompt;
                }
                continue;
            }
            if ((unsigned char)*prompt < 0x20U) {
                /* Control bytes (a legacy leading \n in a linear-mode prompt,
                   BEL, ...) would scroll or garble the pinned input row. */
                ++prompt;
                continue;
            }
            fputc(*prompt, stream);
            ++prompt;
            ++printed;
        }
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
            if ((unsigned char)pending[i] < 0x20U) {
                continue;
            }
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
    tg_tui_resize_flag = 0;
    fputs(TG_UI_CSI "12}", stream); /* unsubscribe the NEWSIZE raw event */
    tg_tui_goto(stream, tg_tui_rows, 1U);
    fputs(TG_UI_CSI "0m\n", stream);
    fflush(stream);
}

static char tg_tui_prompt_text[96];

void tg_console_tui_set_prompt(const char *prompt)
{
    unsigned long i;

    tg_tui_prompt_text[0] = '\0';
    if (prompt == 0) {
        return;
    }
    i = 0UL;
    while (prompt[i] != '\0' && i + 1UL < sizeof(tg_tui_prompt_text)) {
        tg_tui_prompt_text[i] = prompt[i];
        ++i;
    }
    tg_tui_prompt_text[i] = '\0';
}

const char *tg_console_tui_prompt(void)
{
    return tg_tui_prompt_text;
}

FILE *tg_console_tui_capture_begin(FILE *fallback)
{
    FILE *capture;

    if (!tg_tui_active) {
        return fallback;
    }
    capture = tmpfile();
    return capture != 0 ? capture : fallback;
}

void tg_console_tui_capture_end(FILE *capture, FILE *fallback)
{
    char line[512];
    unsigned long length;
    int ch;

    if (capture == 0 || capture == fallback || !tg_tui_active) {
        if (capture != 0 && capture != fallback) {
            fclose(capture);
        }
        return;
    }
    rewind(capture);
    length = 0UL;
    for (;;) {
        ch = fgetc(capture);
        if (ch == EOF || ch == '\n') {
            line[length] = '\0';
            if (length > 0UL || ch == '\n') {
                tg_console_tui_line(fallback, line);
            }
            length = 0UL;
            if (ch == EOF) {
                break;
            }
            continue;
        }
        if (length + 1UL < sizeof(line)) {
            line[length] = (char)ch;
            ++length;
        }
    }
    fclose(capture);
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
    tg_console_tui_line(stream, "resize the window to test NEWSIZE");
    tg_console_tui_input(stream, "you: ", "type q to quit", 14UL);
    for (;;) {
        if (tg_platform_stdin_read_char(60UL, &ch) <= 0) {
            break;
        }
        if (ch == 'q' || ch == 'Q' || ch == 3) {
            break;
        }
        if (ch == (char)0x9b || ch == (char)0x1b) {
            /* Swallow the CSI sequence; react to the NEWSIZE event. */
            unsigned long event_class;
            int byte_value;

            if (ch == (char)0x1b) {
                if (tg_platform_stdin_read_char(1UL, &ch) <= 0 ||
                    ch != '[') {
                    continue;
                }
            }
            event_class = 0UL;
            byte_value = 0;
            for (;;) {
                if (tg_platform_stdin_read_char(1UL, &ch) <= 0) {
                    break;
                }
                byte_value = (int)(unsigned char)ch;
                if (byte_value >= '0' && byte_value <= '9' &&
                    event_class < 1000UL) {
                    event_class = (event_class * 10UL) +
                                  (unsigned long)(byte_value - '0');
                    continue;
                }
                if (byte_value >= 0x40 && byte_value <= 0x7e) {
                    break;
                }
                if (byte_value == ';') {
                    /* Stop accumulating: only the class matters. */
                    event_class += 1000UL;
                }
            }
            if (byte_value == '|' && (event_class % 1000UL) == 12UL) {
                if (tg_console_tui_resize(stream,
                                          " TUI test -- resized ")) {
                    sprintf(line, "resized: new layout drawn");
                    tg_console_tui_line(stream, line);
                }
            }
            tg_console_tui_input(stream, "you: ", "type q to quit", 14UL);
            continue;
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
