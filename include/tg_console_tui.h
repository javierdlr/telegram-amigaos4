/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Full-screen console TUI for the interactive chat: a status bar pinned to
 * the top row, a transcript region that scrolls in the middle and an input
 * line pinned to the bottom row -- the AmIRC-style layout, built only on
 * sequences every Amiga-family console implements.
 *
 * Amiga consoles have no ANSI scroll regions, so the transcript scrolls via
 * the classic insert/delete-line trick: delete the region's top row (rows
 * below shift up, the input row included), then insert a blank row just
 * above the input row (pushing it back down). The window size comes from
 * the console WINDOW STATUS REQUEST (CSI 0 SP q), answered on the input
 * stream as CSI 1;1;<rows>;<cols> SP r.
 */

#ifndef TG_CONSOLE_TUI_H
#define TG_CONSOLE_TUI_H

#include <stdio.h>

/* Master switch: when disabled, tg_console_tui_enter always declines and
   the chat stays on the linear flow (--ui-tui off). */
void tg_console_tui_set_enabled(int enabled);

/* Queries the console window size in characters via CSI 0 SP q. Requires
   stdin in raw mode. Returns 1 and fills rows/columns on success. */
int tg_console_tui_query_size(FILE *stream,
                              unsigned int *rows,
                              unsigned int *columns);

/* Enters the full-screen layout: clears the window, draws the status bar
   and the input separator, homes the input line. Requires raw stdin and a
   window at least 8 rows by 20 columns. Returns 1 when active. */
int tg_console_tui_enter(FILE *stream, const char *status_text);

/* 1 while the full-screen layout is active. */
int tg_console_tui_active(void);

/* Rewrites the top status bar (clipped to the window width). */
void tg_console_tui_status(FILE *stream, const char *status_text);

/* Appends one already-rendered line (no newline) to the transcript region,
   scrolling it. Bytes are written as-is, so callers may embed colour role
   sequences; keep one visual line per call. */
void tg_console_tui_line(FILE *stream, const char *text);

/* Positions the cursor at the input row and redraws prompt + pending input
   (clipped to the window width). */
void tg_console_tui_input(FILE *stream,
                          const char *prompt,
                          const char *pending,
                          unsigned long pending_length);

/* Leaves the layout: moves below the status area and restores attributes. */
void tg_console_tui_leave(FILE *stream);

/* Captures printer output for the transcript region: begin returns a
   temporary stream to print into; end splits what was written into lines
   and feeds each to tg_console_tui_line, then disposes the stream. When the
   TUI is not active, begin returns the fallback stream itself and end is a
   no-op -- so call sites work unchanged in linear mode. */
FILE *tg_console_tui_capture_begin(FILE *fallback);
void tg_console_tui_capture_end(FILE *capture, FILE *fallback);

/* Remembers the prompt text the input row should show; the line editor
   redraws the row with it after every keystroke while the TUI is active. */
void tg_console_tui_set_prompt(const char *prompt);
const char *tg_console_tui_prompt(void);

/* Window-resize handling. enter() subscribes to the console's NEWSIZE raw
   event (CSI 12 {); the line editor calls note_resize() when the event
   report (CSI 12;...|) shows up in the input stream; the chat loop polls
   resize_pending() and calls resize(), which re-queries the window size and
   repaints the chrome and replays the recent transcript lines from the
   in-memory backlog. leave() unsubscribes. */
void tg_console_tui_note_resize(void);
int tg_console_tui_resize_pending(void);
int tg_console_tui_resize(FILE *stream, const char *status_text);

/* Scrollback: pages the transcript view through the in-memory backlog.
   direction > 0 goes back in time, < 0 toward live; the separator row
   doubles as the indicator and any chrome repaint returns to live. */
void tg_console_tui_scroll(FILE *stream, int direction);

/* Interactive diagnostic for --console-tui-test. */
int tg_console_tui_self_test(FILE *stream);

#endif
