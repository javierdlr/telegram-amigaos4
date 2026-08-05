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
static unsigned int tg_tui_composer_rows = 1U;

/* Status is row 1. The composer grows upward from the last row and keeps one
   separator row between itself and the transcript. */
#define TG_TUI_REGION_TOP 2U
#define TG_TUI_MIN_ROWS 8U
#define TG_TUI_MIN_COLUMNS 20U
#define TG_TUI_COMPOSER_MAX_ROWS 3U
#define TG_TUI_WRAP_INDENT 2U

static unsigned int tg_tui_separator_row(void)
{
    return tg_tui_rows - tg_tui_composer_rows;
}

static unsigned int tg_tui_region_bottom(void)
{
    return tg_tui_separator_row() - 1U;
}

static unsigned int tg_tui_region_rows(void)
{
    unsigned int bottom;

    bottom = tg_tui_region_bottom();
    return bottom >= TG_TUI_REGION_TOP
               ? bottom - TG_TUI_REGION_TOP + 1U
               : 0U;
}

static unsigned int tg_tui_composer_top(void)
{
    return tg_tui_separator_row() + 1U;
}

static void tg_tui_goto(FILE *stream, unsigned int row, unsigned int column)
{
    fprintf(stream, TG_UI_CSI "%u;%uH", row, column);
}

/* Reads one CSI report from the console input, skipping unrelated queued
   reports (e.g. NEWSIZE events, CSI 12;...|) until one ends with `final`.
   Returns 1 with the numeric parameters in values/value_count, 0 on timeout
   or when nothing matching shows up within the guard budget. */
static int tg_tui_read_csi_report(char final, unsigned long values[8],
                                  unsigned long *value_count)
{
    char ch;
    unsigned long current;
    int have_digit;
    int guard;

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
        *value_count = 0UL;
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
            if (have_digit && *value_count < 8UL) {
                values[*value_count] = current;
                ++(*value_count);
            }
            current = 0UL;
            have_digit = 0;
            if (ch == ';' || ch == ' ') {
                continue;
            }
            break; /* final byte */
        }
        if (ch != final) {
            /* Another report (e.g. a queued NEWSIZE event, final '|'):
               ignore it and wait for the real answer. */
            continue;
        }
        return 1;
    }
    return 0;
}

int tg_console_tui_query_size(FILE *stream,
                              unsigned int *rows,
                              unsigned int *columns)
{
    unsigned long values[8];
    unsigned long value_count;

    if (stream == 0 || rows == 0 || columns == 0) {
        return 0;
    }
#if defined(__AROS__)
    /* The AROS console does not interpret CSI on OUTPUT while the handler is
       in RAW mode (verified on the modern x86_64/deadwood base: cooked mode
       executes SGR and cursor moves, raw mode DRAWS them as text; the classic
       i386 console never answered the probe either). The chat needs raw for
       per-key input, so a full-screen layout is not attainable here -- and
       sending the probe would only paint "0 q" garbage into the scrollback.
       Skip it: the caller marks the query unanswered, the mini-termcap keeps
       CSI output off, and the linear flow is the honest presentation. */
    return 0;
#endif
    /* Stage 1 -- Amiga WINDOW STATUS REQUEST: the classic console answers on
       the input stream with CSI 1;1;<rows>;<cols> SP r. Understood by every
       console.device descendant (OS3/OS4/MorphOS, AROS i386). */
    fputs(TG_UI_CSI "0 q", stream);
    fflush(stream);
    if (tg_tui_read_csi_report('r', values, &value_count)) {
        if (value_count < 4UL || values[2] < TG_TUI_MIN_ROWS ||
            values[3] < TG_TUI_MIN_COLUMNS || values[2] > 300UL ||
            values[3] > 1000UL) {
            return 0;
        }
        *rows = (unsigned int)values[2];
        *columns = (unsigned int)values[3];
        return 1;
    }
    /* Stage 2 -- ANSI DSR fallback: the modern AROS x86_64 (deadwood) console
       does not implement the Amiga status request (prints it as text) but
       handles core ANSI: park the cursor at the far corner (clamped to the
       window edge) and ask for its position -- the CSI <row>;<col> R report
       IS the window size. No cursor restore needed: the caller either paints
       the full-screen chrome right away or the linear flow scrolls on. */
    fputs(TG_UI_CSI "9999;9999H" TG_UI_CSI "6n", stream);
    fflush(stream);
    if (tg_tui_read_csi_report('R', values, &value_count)) {
        if (value_count < 2UL || values[0] < TG_TUI_MIN_ROWS ||
            values[1] < TG_TUI_MIN_COLUMNS || values[0] > 300UL ||
            values[1] > 1000UL) {
            return 0;
        }
        *rows = (unsigned int)values[0];
        *columns = (unsigned int)values[1];
        return 1;
    }
    return 0;
}

int tg_console_tui_active(void)
{
    return tg_tui_active;
}

/* Returns the byte after a CSI sequence, or NULL when text does not point at
   one. Both the single-byte Amiga CSI and ESC '[' ANSI form are accepted. */
static const char *tg_tui_after_csi(const char *text)
{
    const char *p;

    if (text == 0) {
        return 0;
    }
    p = text;
    if (*p == (char)0x9b) {
        ++p;
    } else if (*p == (char)0x1b && p[1] == '[') {
        p += 2;
    } else {
        return 0;
    }
    while (*p != '\0' &&
           !((unsigned char)*p >= 0x40U &&
             (unsigned char)*p <= 0x7eU)) {
        ++p;
    }
    if (*p != '\0') {
        ++p;
    }
    return p;
}

static unsigned int tg_tui_visible_between(const char *start,
                                           const char *end)
{
    const char *after;
    unsigned int visible;

    visible = 0U;
    while (start != 0 && start < end && *start != '\0') {
        after = tg_tui_after_csi(start);
        if (after != 0) {
            start = after;
            continue;
        }
        if ((unsigned char)*start >= 0x20U) {
            ++visible;
        }
        ++start;
    }
    return visible;
}

typedef struct tg_tui_wrap_piece {
    const char *line_start;
    const char *start;
    const char *end;
    const char *next;
    unsigned int indent;
} tg_tui_wrap_piece;

typedef struct tg_tui_wrap_iter {
    const char *line;
    const char *cursor;
    unsigned int width;
    int first;
    int done;
} tg_tui_wrap_iter;

static void tg_tui_wrap_init(tg_tui_wrap_iter *iter,
                             const char *text,
                             unsigned int width)
{
    iter->line = text != 0 ? text : "";
    iter->cursor = iter->line;
    iter->width = width > 0U ? width : 1U;
    iter->first = 1;
    iter->done = 0;
}

/* Produces one visual row without allocating. A continuation drops the space
   that caused the wrap and reserves two cells for its visual indent. */
static int tg_tui_wrap_next(tg_tui_wrap_iter *iter,
                            tg_tui_wrap_piece *piece)
{
    const char *p;
    const char *scan;
    const char *after;
    const char *last_space;
    const char *next;
    unsigned int visible;
    unsigned int capacity;

    if (iter == 0 || piece == 0 || iter->done) {
        return 0;
    }
    p = iter->cursor;
    if (!iter->first) {
        while (*p == ' ') {
            ++p;
        }
    }
    piece->line_start = iter->line;
    piece->start = p;
    piece->indent = iter->first ? 0U : TG_TUI_WRAP_INDENT;
    if (piece->indent >= iter->width) {
        piece->indent = iter->width > 1U ? iter->width - 1U : 0U;
    }
    capacity = iter->width - piece->indent;
    if (capacity == 0U) {
        capacity = 1U;
    }
    scan = p;
    visible = 0U;
    last_space = 0;
    while (*scan != '\0') {
        after = tg_tui_after_csi(scan);
        if (after != 0) {
            scan = after;
            continue;
        }
        if ((unsigned char)*scan < 0x20U) {
            ++scan;
            continue;
        }
        if (visible >= capacity) {
            break;
        }
        if (*scan == ' ') {
            last_space = scan;
        }
        ++visible;
        ++scan;
    }
    if (*scan == '\0') {
        piece->end = scan;
        piece->next = scan;
        iter->done = 1;
    } else {
        if (*scan == ' ') {
            piece->end = scan;
            next = scan;
        } else if (last_space != 0 && last_space > p) {
            piece->end = last_space;
            next = last_space;
        } else {
            piece->end = scan;
            next = scan;
        }
        while (*next == ' ') {
            ++next;
        }
        /* A line consisting only of control bytes must still make progress. */
        if (piece->end == p && *next != '\0') {
            after = tg_tui_after_csi(next);
            if (after != 0) {
                next = after;
            } else {
                ++next;
            }
            piece->end = next;
        }
        piece->next = next;
        iter->done = (*next == '\0');
    }
    iter->cursor = piece->next;
    iter->first = 0;
    return 1;
}

static unsigned long tg_tui_wrap_count(const char *text,
                                       unsigned int width)
{
    tg_tui_wrap_iter iter;
    tg_tui_wrap_piece piece;
    unsigned long count;

    count = 0UL;
    tg_tui_wrap_init(&iter, text, width);
    while (tg_tui_wrap_next(&iter, &piece)) {
        ++count;
    }
    return count;
}

/* Writes text clipped to the window width, padding with erase-to-EOL. */
static void tg_tui_clipped_line(FILE *stream, const char *text)
{
    const char *after;
    unsigned int printed;

    printed = 0U;
    while (text != 0 && *text != '\0' && printed + 1U < tg_tui_columns) {
        /* Console escape sequences do not consume columns; pass them through
           without counting (introducer to final byte 0x40-0x7e). */
        after = tg_tui_after_csi(text);
        if (after != 0) {
            while (text < after) {
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

/* Paints one wrapped piece. Continuations replay only the colour-control
   prefix that preceded them, so a repaint that starts halfway through one
   logical line still has the right attributes. */
static void tg_tui_wrapped_piece(FILE *stream,
                                 const tg_tui_wrap_piece *piece)
{
    const char *p;
    const char *after;
    unsigned int printed;
    unsigned int i;

    printed = 0U;
    p = piece->line_start;
    while (p < piece->start) {
        after = tg_tui_after_csi(p);
        if (after != 0 && after <= piece->start) {
            while (p < after) {
                fputc(*p, stream);
                ++p;
            }
        } else {
            ++p;
        }
    }
    for (i = 0U; i < piece->indent && printed + 1U < tg_tui_columns; ++i) {
        fputc(' ', stream);
        ++printed;
    }
    p = piece->start;
    while (p < piece->end && printed + 1U < tg_tui_columns) {
        after = tg_tui_after_csi(p);
        if (after != 0 && after <= piece->end) {
            while (p < after) {
                fputc(*p, stream);
                ++p;
            }
            continue;
        }
        if ((unsigned char)*p >= 0x20U) {
            fputc(*p, stream);
            ++printed;
        }
        ++p;
    }
    if (tg_console_ui_color_active() &&
        tg_console_ui_theme() == TG_UI_THEME_DARK) {
        while (printed + 1U < tg_tui_columns) {
            fputc(' ', stream);
            ++printed;
        }
    } else {
        fputs(TG_UI_CSI "K", stream);
    }
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
#ifndef TG_TUI_BACKLOG_WIDTH
#define TG_TUI_BACKLOG_WIDTH 512U
#endif

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

static unsigned int tg_tui_wrap_width(void)
{
    return tg_tui_columns > 1U ? tg_tui_columns - 1U : 1U;
}

/* Scrolls one VIDEO row through the transcript region. */
static void tg_tui_draw_transcript_piece(FILE *stream,
                                         const tg_tui_wrap_piece *piece)
{
    tg_tui_goto(stream, TG_TUI_REGION_TOP, 1U);
    fputs(TG_UI_CSI "M", stream);
    tg_tui_goto(stream, tg_tui_region_bottom(), 1U);
    fputs(TG_UI_CSI "L", stream);
    tg_tui_wrapped_piece(stream, piece);
}

/* The drawing half of tg_console_tui_line (no recording). One logical line
   may insert several video rows on a narrow console. */
static void tg_tui_draw_transcript_line(FILE *stream, const char *text)
{
    tg_tui_wrap_iter iter;
    tg_tui_wrap_piece piece;

    tg_tui_wrap_init(&iter, text, tg_tui_wrap_width());
    while (tg_tui_wrap_next(&iter, &piece)) {
        tg_tui_draw_transcript_piece(stream, &piece);
    }
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
    unsigned long avail = tg_tui_backlog_available();
    unsigned long rows;
    unsigned long kept;
    unsigned long index;
    unsigned int region;

    region = tg_tui_region_rows();
    if (region == 0U || avail == 0UL) {
        return 0UL;
    }
    rows = 0UL;
    kept = 0UL;
    index = tg_tui_backlog_total - avail;
    while (kept < avail && rows < (unsigned long)region) {
        rows += tg_tui_wrap_count(
            tg_tui_backlog[(index + kept) % TG_TUI_BACKLOG_LINES],
            tg_tui_wrap_width());
        ++kept;
    }
    return avail > kept ? avail - kept : 0UL;
}

/* Redraws the whole transcript region from the backlog at the current view
   offset; the separator doubles as the scrollback indicator. */
static void tg_tui_redraw_region(FILE *stream)
{
    unsigned long avail = tg_tui_backlog_available();
    unsigned long selected;
    unsigned long remaining;
    unsigned long skip_oldest;
    unsigned long back;
    unsigned long i;
    unsigned long r;
    unsigned int region;
    unsigned int row;

    region = tg_tui_region_rows();
    if (region == 0U) {
        return;
    }
    if (tg_tui_view_offset > tg_tui_view_offset_max()) {
        tg_tui_view_offset = tg_tui_view_offset_max();
    }
    for (r = 0UL; r < (unsigned long)region; ++r) {
        tg_tui_goto(stream, TG_TUI_REGION_TOP + (unsigned int)r, 1U);
        tg_tui_clipped_line(stream, "");
    }

    /* Select logical lines from newest to oldest until their wrapped video
       rows fill the region. Only the oldest selected line may be clipped. */
    selected = 0UL;
    remaining = (unsigned long)region;
    skip_oldest = 0UL;
    back = tg_tui_view_offset;
    while (back < avail && remaining > 0UL) {
        unsigned long index;
        unsigned long pieces;

        index = tg_tui_backlog_total - 1UL - back;
        pieces = tg_tui_wrap_count(
            tg_tui_backlog[index % TG_TUI_BACKLOG_LINES],
            tg_tui_wrap_width());
        ++selected;
        if (pieces >= remaining) {
            skip_oldest = pieces - remaining;
            remaining = 0UL;
            break;
        }
        remaining -= pieces;
        ++back;
    }
    row = TG_TUI_REGION_TOP + (unsigned int)remaining;
    for (i = selected; i > 0UL; --i) {
        unsigned long logical_back;
        unsigned long index;
        unsigned long piece_index;
        unsigned long skip;
        tg_tui_wrap_iter iter;
        tg_tui_wrap_piece piece;

        logical_back = tg_tui_view_offset + i - 1UL;
        index = tg_tui_backlog_total - 1UL - logical_back;
        skip = (i == selected) ? skip_oldest : 0UL;
        piece_index = 0UL;
        tg_tui_wrap_init(
            &iter, tg_tui_backlog[index % TG_TUI_BACKLOG_LINES],
            tg_tui_wrap_width());
        while (tg_tui_wrap_next(&iter, &piece)) {
            if (piece_index >= skip && row <= tg_tui_region_bottom()) {
                tg_tui_goto(stream, row, 1U);
                tg_tui_wrapped_piece(stream, &piece);
                ++row;
            }
            ++piece_index;
        }
    }
    tg_tui_goto(stream, tg_tui_separator_row(), 1U);
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
    /* Scroll by logical messages. Their wrapped video-row count is resolved
       afresh at paint time, so a resize reflows the same history. */
    step = (unsigned long)tg_tui_region_rows() / 2UL;
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

static void tg_tui_paint_chrome(FILE *stream, const char *status_text)
{
    unsigned int row;

    /* A chrome repaint (enter, resize, /add round-trip) returns to live. */
    tg_tui_view_offset = 0UL;
    /* Base attributes + full clear, then the fixed chrome. */
    tg_console_ui_reset(stream);
    fputs(TG_UI_CSI "H" TG_UI_CSI "J", stream);
    tg_console_tui_status(stream, status_text);
    /* What was on screen, back on screen: the repaint should cost the user
       nothing but a blink. */
    tg_tui_redraw_region(stream);
    for (row = tg_tui_composer_top(); row <= tg_tui_rows; ++row) {
        tg_tui_goto(stream, row, 1U);
        fputs(TG_UI_CSI "K", stream);
    }
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
    tg_tui_composer_rows = 1U;
    tg_tui_active = 1;
    tg_tui_resize_flag = 0;
    /* Subscribe to the console's NEWSIZE (12) and CLOSEWINDOW (11) raw
       events: resizes arrive on stdin as CSI 12;...| reports (turned into a
       pending-resize flag) and a close-gadget click as CSI 11;...| (turned
       into a clean quit by the line editor). */
    /* One class per sequence: the original ROM 3.1 console handler mishandles
       the combined "11;12{" subscription (field report 2026-08-05: close
       gadget dead on a plain 3.1 68000). Single-class sequences register
       fine on every console flavour, so send one per class. */
    fputs(TG_UI_CSI "11{", stream);
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

/* Visible column count of a prompt, honouring the same rules as the painter
   below (colour sequences and control bytes take no columns). */
static unsigned int tg_tui_prompt_columns(const char *prompt)
{
    const char *after;
    unsigned int printed = 0U;

    if (prompt == 0) {
        return 0U;
    }
    while (*prompt != '\0') {
        after = tg_tui_after_csi(prompt);
        if (after != 0) {
            prompt = after;
            continue;
        }
        if ((unsigned char)*prompt >= 0x20U) {
            ++printed;
        }
        ++prompt;
    }
    return printed;
}

/* Fast caret-at-end echo: on a 7 MHz 68000 the full clear-and-repaint of the
   input row flashes on every keystroke (field report 2026-08-05). When the
   whole line still fits the row, the hardware cursor already rests right
   after the text, so echoing the new character is enough. Returns 0 when the
   caller must fall back to the full repaint (row scrolled, control char). */
int tg_console_tui_input_append(FILE *stream,
                                const char *prompt,
                                unsigned long pending_length,
                                char ch)
{
    if (stream == 0 || !tg_tui_active || tg_tui_composer_rows != 1U ||
        (unsigned char)ch < 0x20U) {
        return 0;
    }
    if (tg_tui_prompt_columns(prompt) + pending_length + 2UL >
        (unsigned long)tg_tui_columns) {
        return 0; /* tail-scroll mode: only the repaint knows the window */
    }
    fputc(ch, stream);
    fflush(stream);
    return 1;
}

/* Fast caret-at-end backspace: same conditions as the append above. */
int tg_console_tui_input_backspace(FILE *stream,
                                   const char *prompt,
                                   unsigned long pending_length)
{
    if (stream == 0 || !tg_tui_active || tg_tui_composer_rows != 1U) {
        return 0;
    }
    if (tg_tui_prompt_columns(prompt) + pending_length + 3UL >
        (unsigned long)tg_tui_columns) {
        return 0; /* the repaint must re-reveal the scrolled-out head */
    }
    fputs("\b \b", stream);
    fflush(stream);
    return 1;
}

#define TG_TUI_INPUT_TEXT_MAX 640U

typedef struct tg_tui_composer_plan {
    unsigned long total_pieces;
    unsigned long first_piece;
    unsigned long caret_piece;
    unsigned int rows;
    unsigned int caret_column;
} tg_tui_composer_plan;

static unsigned long tg_tui_build_input_text(
    char out[TG_TUI_INPUT_TEXT_MAX],
    const char *prompt,
    const char *pending,
    unsigned long pending_length,
    unsigned long pending_caret,
    unsigned long *caret_offset)
{
    unsigned long length;
    unsigned long i;

    length = 0UL;
    if (prompt != 0) {
        while (*prompt != '\0' && length + 1UL < TG_TUI_INPUT_TEXT_MAX) {
            out[length++] = *prompt++;
        }
    }
    if (pending_caret > pending_length) {
        pending_caret = pending_length;
    }
    *caret_offset = length + pending_caret;
    if (pending != 0) {
        for (i = 0UL; i < pending_length &&
                          length + 1UL < TG_TUI_INPUT_TEXT_MAX;
             ++i) {
            out[length++] = pending[i];
        }
    }
    out[length] = '\0';
    if (*caret_offset > length) {
        *caret_offset = length;
    }
    return length;
}

/* Pure composer layout: used by both the painter and the host golden test. */
static void tg_tui_make_composer_plan(const char *text,
                                      unsigned long caret_offset,
                                      unsigned int width,
                                      tg_tui_composer_plan *plan)
{
    const char *caret;
    const char *text_end;
    tg_tui_wrap_iter iter;
    tg_tui_wrap_piece piece;
    unsigned long index;
    int caret_found;

    memset(plan, 0, sizeof(*plan));
    text_end = text + strlen(text);
    if (caret_offset > (unsigned long)(text_end - text)) {
        caret_offset = (unsigned long)(text_end - text);
    }
    caret = text + caret_offset;
    caret_found = 0;
    index = 0UL;
    tg_tui_wrap_init(&iter, text, width);
    while (tg_tui_wrap_next(&iter, &piece)) {
        if (!caret_found && caret >= piece.start && caret < piece.end) {
            plan->caret_piece = index;
            plan->caret_column = piece.indent +
                tg_tui_visible_between(piece.start, caret) + 1U;
            caret_found = 1;
        } else if (!caret_found && !iter.done && caret >= piece.end &&
                   caret < piece.next) {
            plan->caret_piece = index + 1UL;
            plan->caret_column = TG_TUI_WRAP_INDENT + 1U;
            caret_found = 1;
        } else if (!caret_found && iter.done && caret >= piece.end) {
            plan->caret_piece = index;
            plan->caret_column = piece.indent +
                tg_tui_visible_between(piece.start, piece.end) + 1U;
            caret_found = 1;
        }
        ++index;
    }
    plan->total_pieces = index > 0UL ? index : 1UL;
    if (!caret_found) {
        plan->caret_piece = plan->total_pieces - 1UL;
        plan->caret_column = 1U;
    }
    plan->rows = plan->total_pieces > TG_TUI_COMPOSER_MAX_ROWS
                     ? TG_TUI_COMPOSER_MAX_ROWS
                     : (unsigned int)plan->total_pieces;
    if (plan->total_pieces > (unsigned long)plan->rows) {
        if (plan->caret_piece + 1UL > (unsigned long)plan->rows) {
            plan->first_piece =
                plan->caret_piece + 1UL - (unsigned long)plan->rows;
        }
        if (plan->first_piece + (unsigned long)plan->rows >
            plan->total_pieces) {
            plan->first_piece =
                plan->total_pieces - (unsigned long)plan->rows;
        }
    }
    if (plan->caret_piece < plan->first_piece) {
        plan->caret_piece = plan->first_piece;
    } else if (plan->caret_piece >=
               plan->first_piece + (unsigned long)plan->rows) {
        plan->caret_piece =
            plan->first_piece + (unsigned long)plan->rows - 1UL;
    }
    if (plan->caret_column > width + 1U) {
        plan->caret_column = width + 1U;
    }
}

void tg_console_tui_input_caret(FILE *stream,
                                const char *prompt,
                                const char *pending,
                                unsigned long pending_length,
                                unsigned long pending_caret)
{
    char text[TG_TUI_INPUT_TEXT_MAX];
    unsigned long caret_offset;
    unsigned long piece_index;
    unsigned int row;
    unsigned int old_rows;
    tg_tui_composer_plan plan;
    tg_tui_wrap_iter iter;
    tg_tui_wrap_piece piece;

    if (stream == 0 || !tg_tui_active) {
        return;
    }
    (void)tg_tui_build_input_text(text, prompt, pending, pending_length,
                                  pending_caret, &caret_offset);
    tg_tui_make_composer_plan(text, caret_offset, tg_tui_wrap_width(), &plan);
    old_rows = tg_tui_composer_rows;
    tg_tui_composer_rows = plan.rows;
    if (old_rows != tg_tui_composer_rows) {
        /* Only a 1/2/3-row threshold crossing changes transcript geometry. */
        tg_tui_redraw_region(stream);
    }
    for (row = tg_tui_composer_top(); row <= tg_tui_rows; ++row) {
        tg_tui_goto(stream, row, 1U);
        fputs(TG_UI_CSI "K", stream);
    }
    row = tg_tui_composer_top();
    piece_index = 0UL;
    tg_tui_wrap_init(&iter, text, tg_tui_wrap_width());
    while (tg_tui_wrap_next(&iter, &piece)) {
        if (piece_index >= plan.first_piece &&
            piece_index < plan.first_piece + (unsigned long)plan.rows) {
            tg_tui_goto(stream, row, 1U);
            tg_tui_wrapped_piece(stream, &piece);
            ++row;
        }
        ++piece_index;
    }
    row = tg_tui_composer_top() +
          (unsigned int)(plan.caret_piece - plan.first_piece);
    tg_tui_goto(stream, row, plan.caret_column);
    fflush(stream);
}

void tg_console_tui_input(FILE *stream,
                          const char *prompt,
                          const char *pending,
                          unsigned long pending_length)
{
    tg_console_tui_input_caret(stream, prompt, pending, pending_length,
                               pending_length);
}

void tg_console_tui_leave(FILE *stream)
{
    if (stream == 0 || !tg_tui_active) {
        return;
    }
    tg_tui_active = 0;
    tg_tui_resize_flag = 0;
    tg_tui_composer_rows = 1U;
    fputs(TG_UI_CSI "11}", stream); /* unsubscribe CLOSE raw events */
    fputs(TG_UI_CSI "12}", stream); /* unsubscribe NEWSIZE raw events */
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

static void tg_tui_collect_wrap(const char *text,
                                unsigned int columns,
                                char *out,
                                unsigned long out_size)
{
    tg_tui_wrap_iter iter;
    tg_tui_wrap_piece piece;
    const char *p;
    const char *after;
    unsigned long length;
    unsigned int i;
    int first;

    length = 0UL;
    first = 1;
    if (out_size == 0UL) {
        return;
    }
    tg_tui_wrap_init(&iter, text, columns > 1U ? columns - 1U : 1U);
    while (tg_tui_wrap_next(&iter, &piece)) {
        if (!first && length + 1UL < out_size) {
            out[length++] = '|';
        }
        first = 0;
        for (i = 0U; i < piece.indent && length + 1UL < out_size; ++i) {
            out[length++] = ' ';
        }
        p = piece.start;
        while (p < piece.end && length + 1UL < out_size) {
            after = tg_tui_after_csi(p);
            if (after != 0 && after <= piece.end) {
                p = after;
                continue;
            }
            if ((unsigned char)*p >= 0x20U) {
                out[length++] = *p;
            }
            ++p;
        }
    }
    out[length] = '\0';
}

static int tg_tui_expect_wrap(const char *label,
                              const char *text,
                              unsigned int columns,
                              const char *expected)
{
    char actual[640];

    tg_tui_collect_wrap(text, columns, actual, sizeof(actual));
    if (strcmp(actual, expected) != 0) {
        printf("tui layout self-test: %s mismatch\nexpected: %s\nactual:   %s\n",
               label, expected, actual);
        return 0;
    }
    return 1;
}

int tg_console_tui_layout_self_test(void)
{
    static const char words[] =
        "0123456789 1234567890 1234567890 1234567890 1234567890 "
        "1234567890";
    static const char coloured[] =
        TG_UI_CSI "31m0123456789 1234567890 1234567890 1234567890 "
        "1234567890 1234567890" TG_UI_CSI "0m";
    char monster[51];
    char composer[161];
    tg_tui_composer_plan plan;
    unsigned int i;

    if (!tg_tui_expect_wrap(
            "40 columns", words, 40U,
            "0123456789 1234567890 1234567890|  1234567890 1234567890 "
            "1234567890") ||
        !tg_tui_expect_wrap(
            "53 columns", words, 53U,
            "0123456789 1234567890 1234567890 1234567890|  1234567890 "
            "1234567890") ||
        !tg_tui_expect_wrap("80 columns", words, 80U, words) ||
        !tg_tui_expect_wrap(
            "colour width", coloured, 40U,
            "0123456789 1234567890 1234567890|  1234567890 1234567890 "
            "1234567890")) {
        return 2;
    }
    for (i = 0U; i < 50U; ++i) {
        monster[i] = 'x';
    }
    monster[50] = '\0';
    if (!tg_tui_expect_wrap(
            "hard word", monster, 40U,
            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|  xxxxxxxxxxx")) {
        return 2;
    }

    for (i = 0U; i < 160U; ++i) {
        composer[i] = 'x';
    }
    composer[160] = '\0';
    composer[10] = '\0';
    tg_tui_make_composer_plan(composer, 10UL, 39U, &plan);
    if (plan.rows != 1U || plan.caret_piece != 0UL ||
        plan.caret_column != 11U) {
        puts("tui layout self-test: composer one-row mismatch");
        return 2;
    }
    composer[10] = 'x';
    composer[50] = '\0';
    tg_tui_make_composer_plan(composer, 50UL, 39U, &plan);
    if (plan.rows != 2U || plan.caret_piece != 1UL) {
        puts("tui layout self-test: composer two-row mismatch");
        return 2;
    }
    composer[50] = 'x';
    composer[90] = '\0';
    tg_tui_make_composer_plan(composer, 90UL, 39U, &plan);
    if (plan.rows != 3U || plan.caret_piece != 2UL) {
        puts("tui layout self-test: composer three-row mismatch");
        return 2;
    }
    for (i = 90U; i < 160U; ++i) {
        composer[i] = 'x';
    }
    composer[160] = '\0';
    tg_tui_make_composer_plan(composer, 160UL, 39U, &plan);
    if (plan.rows != 3U || plan.total_pieces != 5UL ||
        plan.first_piece != 2UL || plan.caret_piece != 4UL) {
        puts("tui layout self-test: composer tail viewport mismatch");
        return 2;
    }
    tg_tui_make_composer_plan("short", 5UL, 39U, &plan);
    if (plan.rows != 1U || plan.first_piece != 0UL) {
        puts("tui layout self-test: composer shrink mismatch");
        return 2;
    }
    puts("tui layout self-test: ok (40/53/80 wrap + 1/2/3-row composer)");
    return 0;
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
