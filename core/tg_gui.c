/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Portable model + renderer for the native GUI line. The per-platform backend
 * supplies only metrics and draw primitives (see tg_gui.h); everything about
 * layout lives here so it can be unit-tested on the host and shared by every
 * Amiga backend. Redraw discipline (only changed rows) comes later with the
 * real window; this file establishes the full-paint geometry first.
 */

#include "tg_gui.h"

#include <stdio.h>
#include <string.h>

#define TG_GUI_WRAP_MAX_LINES 16

/* Custom-drawn vertical scrollbar geometry (no GadTools propgadget, so it is
   identical on every backend). TG_GUI_SCROLLBAR_W lives in tg_gui.h, shared
   with the event loop's knob hit-test. */
#define TG_GUI_SCROLLBAR_MIN_KNOB 14

/* When 0, tg_gui_paint skips the leading full-window clear so a repeated repaint
   of unchanged opaque content does not flash the window (used by the redraw-time
   measurement). Default on. */
static int tg_gui_clear_background = 1;

void tg_gui_set_background_clear(int enabled)
{
    tg_gui_clear_background = enabled ? 1 : 0;
}

static void tg_gui_copy(char *dest, unsigned long size, const char *src)
{
    unsigned long i;

    if (dest == 0 || size == 0UL) {
        return;
    }
    if (src == 0) {
        dest[0] = '\0';
        return;
    }
    for (i = 0UL; i + 1UL < size && src[i] != '\0'; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static void tg_gui_set_chat(tg_gui_chat *chat, const char *name,
                            const char *preview, const char *time,
                            const char *initials, int avatar_color, int unread)
{
    tg_gui_copy(chat->name, sizeof(chat->name), name);
    tg_gui_copy(chat->preview, sizeof(chat->preview), preview);
    tg_gui_copy(chat->time, sizeof(chat->time), time);
    tg_gui_copy(chat->initials, sizeof(chat->initials), initials);
    chat->avatar_color = avatar_color;
    chat->unread = unread;
}

static void tg_gui_set_message(tg_gui_message *message, const char *sender,
                               const char *text, const char *time,
                               int sender_color, int is_own, int is_system)
{
    tg_gui_copy(message->sender, sizeof(message->sender), sender);
    tg_gui_copy(message->text, sizeof(message->text), text);
    tg_gui_copy(message->time, sizeof(message->time), time);
    message->sender_color = sender_color;
    message->is_own = is_own;
    message->is_system = is_system;
    message->reply_text[0] = '\0'; /* demo carries no replies */
}

void tg_gui_demo_state(tg_gui_state *state)
{
    if (state == 0) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->theme = TG_GUI_THEME_DARK;

    tg_gui_set_chat(&state->chats[0], "Sviluppo AmigaIta",
                    "Tu: ottimo, la pausa-bozza ha fatto il suo", "09:20",
                    "SA", 0, 0);
    tg_gui_set_chat(&state->chats[1], "Mario Rossi", "ci vediamo domani allora",
                    "13:10", "MR", 1, 2);
    tg_gui_set_chat(&state->chats[2], "MorphOS Team",
                    "Giulia: build pronta da provare", "12:48", "MT", 2, 7);
    tg_gui_set_chat(&state->chats[3], "Anna", "perfetto, grazie mille", "11:20",
                    "A", 3, 0);
    tg_gui_set_chat(&state->chats[4], "Amiga News",
                    "Nuovo update di sistema disponibile", "ieri", "AN", 4, 0);
    state->chat_count = 5;
    state->selected_chat = 0;

    tg_gui_copy(state->title, sizeof(state->title), "Sviluppo AmigaIta");
    tg_gui_copy(state->subtitle, sizeof(state->subtitle), "group - 128 members");

    tg_gui_set_message(&state->messages[0], "Henry Out",
                       "Sul 030 a 25 MHz ora si scrive fluido, niente piu' "
                       "scatti tra un tasto e l'altro.",
                       "09:14", 1, 0, 0);
    tg_gui_set_message(&state->messages[1], "Lallo",
                       "Confermo anche su A1200 con Blizzard 060.", "09:16", 2,
                       0, 0);
    tg_gui_set_message(&state->messages[2], "",
                       "Ottimo, allora la pausa-bozza ha fatto il suo lavoro.",
                       "09:20", 0, 1, 0);
    tg_gui_set_message(&state->messages[3], "", "Marco e' entrato nel gruppo",
                       "", 0, 0, 1);
    /* The own message carries a read receipt so the demo shows the double-check
       (the peer has read it); incoming/system rows keep no mark. */
    state->messages[2].read_state = TG_GUI_READ_SEEN;
    state->message_count = 4;

    tg_gui_copy(state->input, sizeof(state->input), "");
    tg_gui_copy(state->status, sizeof(state->status), "Connected - DC4");
}

/* Word-wraps text to max_width using the backend's font metrics, filling the
   starts/lengths arrays with up to max_lines segments. Returns the line count.
   Always makes progress (at least one character per line) so a glyph wider than
   max_width cannot loop forever. */
static int tg_gui_wrap(tg_gui_backend *backend, const char *text, int max_width,
                       unsigned long *starts, unsigned long *lengths,
                       int max_lines)
{
    unsigned long total;
    unsigned long i;
    int line;

    total = (unsigned long)strlen(text);
    i = 0UL;
    line = 0;
    while (i < total && line < max_lines) {
        unsigned long line_start;
        unsigned long last_space;
        int have_space;
        unsigned long j;

        line_start = i;
        last_space = 0UL;
        have_space = 0;
        j = i;
        while (j < total) {
            unsigned long segment;

            segment = j - line_start + 1UL;
            if (j > line_start &&
                backend->text_width(backend, text + line_start, segment) >
                    max_width) {
                break;
            }
            if (text[j] == ' ') {
                last_space = j;
                have_space = 1;
            }
            ++j;
        }
        if (j >= total) {
            starts[line] = line_start;
            lengths[line] = total - line_start;
            ++line;
            break;
        }
        if (have_space && last_space > line_start) {
            starts[line] = line_start;
            lengths[line] = last_space - line_start;
            i = last_space + 1UL;
        } else {
            starts[line] = line_start;
            lengths[line] = j - line_start;
            i = j;
        }
        ++line;
    }
    return line;
}

static tg_gui_rect tg_gui_make_rect(int x, int y, int w, int h)
{
    tg_gui_rect rect;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    return rect;
}

/* Draws text but never wider than max_w, appending ".." when it has to cut.
   Columns (the chat list especially) must clip horizontally: the backends
   draw to a single RastPort with no per-column clip, so an unclipped long
   preview would bleed straight across the sidebar boundary into the
   conversation pane. */
static void tg_gui_draw_clipped(tg_gui_backend *backend, int pen, int x,
                                int baseline, const char *text, int max_w)
{
    unsigned long n;
    int dots_w;
    int budget;
    unsigned long fit;

    if (text == 0 || max_w <= 0) {
        return;
    }
    n = (unsigned long)strlen(text);
    if (n == 0UL) {
        return;
    }
    if (backend->text_width(backend, text, n) <= max_w) {
        backend->draw_text(backend, pen, x, baseline, text, n);
        return;
    }
    dots_w = backend->text_width(backend, "..", 2UL);
    budget = max_w - dots_w;
    if (budget <= 0) {
        return; /* not even room for the ellipsis */
    }
    fit = n;
    while (fit > 0UL &&
           backend->text_width(backend, text, fit) > budget) {
        --fit;
    }
    if (fit > 0UL) {
        backend->draw_text(backend, pen, x, baseline, text, fit);
        backend->draw_text(backend, pen,
                           x + backend->text_width(backend, text, fit), baseline,
                           "..", 2UL);
    }
}

/* Draws a vertical scrollbar: a full-height track with a proportional knob.
   total/view/offset are in the panel's own units (chat rows or messages);
   offset is measured from the top (0..total-view). No knob when all fits. */
static void tg_gui_paint_scrollbar(tg_gui_backend *backend, int x, int track_y,
                                   int track_h, int total, int view, int offset,
                                   int *out_knob_y, int *out_knob_h)
{
    int knob_h;
    int knob_y;
    int span;
    int max_off;

    knob_y = track_y;
    knob_h = (track_h > 0) ? track_h : 0;
    if (track_h <= 0) {
        if (out_knob_y) {
            *out_knob_y = knob_y;
        }
        if (out_knob_h) {
            *out_knob_h = knob_h;
        }
        return;
    }
    backend->fill_rect(backend, TG_GUI_PEN_SELECT,
                       tg_gui_make_rect(x, track_y, TG_GUI_SCROLLBAR_W, track_h));
    if (total <= view || view <= 0) {
        /* Everything fits: inert full track, no draggable knob. */
        if (out_knob_y) {
            *out_knob_y = track_y;
        }
        if (out_knob_h) {
            *out_knob_h = track_h;
        }
        return;
    }
    knob_h = (track_h * view) / total;
    if (knob_h < TG_GUI_SCROLLBAR_MIN_KNOB) {
        knob_h = TG_GUI_SCROLLBAR_MIN_KNOB;
    }
    if (knob_h > track_h) {
        knob_h = track_h;
    }
    span = track_h - knob_h;
    max_off = total - view;
    if (offset < 0) {
        offset = 0;
    }
    if (offset > max_off) {
        offset = max_off;
    }
    knob_y = track_y + (max_off > 0 ? (span * offset) / max_off : 0);
    backend->fill_rect(backend, TG_GUI_PEN_TEXT_DIM,
                       tg_gui_make_rect(x + 2, knob_y, TG_GUI_SCROLLBAR_W - 4,
                                        knob_h));
    if (out_knob_y) {
        *out_knob_y = knob_y;
    }
    if (out_knob_h) {
        *out_knob_h = knob_h;
    }
}

static void tg_gui_paint_sidebar(const tg_gui_state *state,
                                 tg_gui_backend *backend, int sidebar_w,
                                 int content_h, int lh)
{
    int search_h;
    int row_h;
    int y;
    int i;
    int view_rows;
    int need_bar;
    int list_w;
    tg_gui_state *st;

    backend->fill_rect(backend, TG_GUI_PEN_SURFACE,
                       tg_gui_make_rect(0, 0, sidebar_w, content_h));

    search_h = lh + 10;
    backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM, 10, (search_h / 2) + 4,
                       "Search chats...", 15UL);

    row_h = (2 * lh) + 12;
    view_rows = (row_h > 0) ? ((content_h - search_h) / row_h) : 0;
    if (view_rows < 1) {
        view_rows = 1;
    }
    need_bar = state->chat_count > view_rows;
    list_w = need_bar ? (sidebar_w - TG_GUI_SCROLLBAR_W) : sidebar_w;
    /* The painter owns the geometry, so it clamps the scroll offset the event
       loop advanced freely (cast away const to write the model's own field). */
    st = (tg_gui_state *)state;
    if (st->chat_scroll > state->chat_count - view_rows) {
        st->chat_scroll = state->chat_count - view_rows;
    }
    if (st->chat_scroll < 0) {
        st->chat_scroll = 0;
    }
    y = search_h;
    for (i = state->chat_scroll;
         i < state->chat_count && y + row_h <= content_h; ++i) {
        const tg_gui_chat *chat;
        int avatar;
        int text_x;
        int name_pen;
        int preview_pen;

        chat = &state->chats[i];
        /* The selected row is set apart by its tint and accent bar; the text
           pens are the same as an unselected row. */
        name_pen = TG_GUI_PEN_TEXT;
        preview_pen = TG_GUI_PEN_TEXT_DIM;
        if (i == state->selected_chat) {
            backend->fill_rect(backend, TG_GUI_PEN_SELECT,
                               tg_gui_make_rect(0, y, sidebar_w, row_h));
            backend->fill_rect(backend, TG_GUI_PEN_ACCENT,
                               tg_gui_make_rect(0, y, 3, row_h));
        }

        avatar = (2 * lh);
        backend->avatar_fill(backend, chat->avatar_color,
                             tg_gui_make_rect(8, y + 6, avatar, avatar));
        backend->draw_text(backend, TG_GUI_PEN_TEXT, 8 + 6, y + 6 + lh,
                           chat->initials, (unsigned long)strlen(chat->initials));

        text_x = 8 + avatar + 8;
        {
            char badge[8];
            int badge_w;
            int badge_x;
            int has_preview;
            int row_mid;
            int name_baseline;
            int name_limit;

            /* The peer cache carries no last-message preview, so a row is a
               single line: the name and the unread badge sit on the avatar's
               vertical centre (level with the initials), instead of name-on-top
               with the badge a line below. When a preview is present (a future
               per-chat fill) the row keeps the two-line name-over-preview
               layout with the time top-right. */
            has_preview = chat->preview[0] != '\0';
            row_mid = y + 6 + lh; /* avatar centre == initials baseline */
            /* Single line: drop the name a couple of pixels below the initials
               baseline so it sits at the optical centre between the avatar and
               badge bubbles (the bitmap-font descent makes a baseline-aligned
               name read high). */
            name_baseline = has_preview ? (y + 4 + lh) : (row_mid + 2);

            badge[0] = '\0';
            badge_w = 0;
            badge_x = list_w - 10;
            if (chat->unread > 0) {
                int num_w;

                sprintf(badge, "%d", chat->unread > 999 ? 999 : chat->unread);
                num_w = backend->text_width(backend, badge,
                                            (unsigned long)strlen(badge));
                badge_w = num_w + 10; /* horizontal padding inside the pill */
                if (badge_w < lh + 6) {
                    badge_w = lh + 6; /* keep it pill-shaped for one digit */
                }
                badge_x = list_w - 8 - badge_w;
            }
            /* Name clips before the time column (two-line) or before the badge
               / row edge (single-line). */
            if (has_preview) {
                name_limit = list_w - 38;
            } else {
                name_limit = (chat->unread > 0) ? (badge_x - 6)
                                                : (list_w - 10);
            }
            tg_gui_draw_clipped(backend, name_pen, text_x, name_baseline,
                                chat->name, name_limit - text_x);
            if (has_preview) {
                int preview_limit;

                backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM, sidebar_w - 34,
                                   y + 4 + lh, chat->time,
                                   (unsigned long)strlen(chat->time));
                preview_limit = (chat->unread > 0) ? (badge_x - 4)
                                                   : (sidebar_w - 10);
                tg_gui_draw_clipped(backend, preview_pen, text_x,
                                    y + 8 + (2 * lh), chat->preview,
                                    preview_limit - text_x);
            }
            if (chat->unread > 0) {
                int badge_top;
                int badge_h;
                int num_x;

                badge_h = lh + 4;
                /* Two-line: pill on the preview line. Single-line: pill centred
                   on the row middle, level with the name and avatar. The count
                   stays inside the fill either way. */
                badge_top = has_preview ? ((y + 8 + (2 * lh)) - lh)
                                        : (row_mid - (badge_h / 2));
                /* A chat that just got a notification draws its badge in the
                   accent pen to stand out; the live event loop toggles
                   chat->flash for a true blink. */
                backend->fill_rect(backend,
                                   chat->flash ? TG_GUI_PEN_ACCENT
                                               : TG_GUI_PEN_BADGE,
                                   tg_gui_make_rect(badge_x, badge_top, badge_w,
                                                    badge_h));
                num_x = badge_x +
                        (badge_w - backend->text_width(
                                       backend, badge,
                                       (unsigned long)strlen(badge))) /
                            2;
                backend->draw_text(backend, TG_GUI_PEN_BADGE_TEXT, num_x,
                                   badge_top + lh, badge,
                                   (unsigned long)strlen(badge));
            }
        }
        y += row_h;
    }
    if (need_bar) {
        int ky;
        int kh;

        tg_gui_paint_scrollbar(backend, sidebar_w - TG_GUI_SCROLLBAR_W, search_h,
                               content_h - search_h, state->chat_count,
                               view_rows, state->chat_scroll, &ky, &kh);
        st->sb_list_x = sidebar_w - TG_GUI_SCROLLBAR_W;
        st->sb_list_ty = search_h;
        st->sb_list_th = content_h - search_h;
        st->sb_list_ky = ky;
        st->sb_list_kh = kh;
        st->sb_list_max = state->chat_count - view_rows;
    } else {
        st->sb_list_max = 0;
    }
}

/* Draws one wrapped line of message text, interpreting the inline markup
   markers (* _ ` ~) as styling: each marker toggles a TG_GUI_STYLE_* bit, is
   not drawn itself, and the run between markers is drawn in the current style
   via the backend's set_style hook. *style carries across wrapped lines so a
   span that breaks over two lines stays styled. Backends without set_style get
   clean text (markers skipped), just unstyled. */
static void tg_gui_draw_markup(tg_gui_backend *backend, int pen, int x,
                               int baseline, const char *text,
                               unsigned long length, int *style)
{
    unsigned long run_start = 0UL;
    unsigned long i;

    for (i = 0UL; i <= length; ++i) {
        int toggle = 0;

        if (i < length) {
            switch (text[i]) {
            case '*':
                toggle = TG_GUI_STYLE_BOLD;
                break;
            case '_':
                toggle = TG_GUI_STYLE_ITALIC;
                break;
            case '`':
                toggle = TG_GUI_STYLE_CODE;
                break;
            case '~':
                toggle = TG_GUI_STYLE_STRIKE;
                break;
            default:
                break;
            }
        }
        if (toggle != 0 || i == length) {
            if (i > run_start) {
                if (backend->set_style != 0) {
                    backend->set_style(backend, *style);
                }
                backend->draw_text(backend, pen, x, baseline,
                                   text + run_start, i - run_start);
                x += backend->text_width(backend, text + run_start,
                                         i - run_start);
            }
            if (toggle != 0) {
                *style ^= toggle;
                run_start = i + 1UL;
            }
        }
    }
}

/* The read-receipt mark drawn after an own message's timestamp. ASCII, since
   the bitmap fonts carry no Unicode check glyph: "v" = delivered (sent), "vv" =
   read by the peer (seen). Empty for incoming messages and unsent echoes. */
static const char *tg_gui_read_mark(const tg_gui_message *message)
{
    if (message->is_own) {
        if (message->read_state == TG_GUI_READ_SEEN) {
            return "vv";
        }
        if (message->read_state == TG_GUI_READ_SENT) {
            return "v";
        }
    }
    return "";
}

static int tg_gui_paint_bubble(tg_gui_backend *backend,
                               const tg_gui_message *message, int area_x,
                               int area_w, int y, int lh, int bottom)
{
    int style;
    unsigned long starts[TG_GUI_WRAP_MAX_LINES];
    unsigned long lengths[TG_GUI_WRAP_MAX_LINES];
    int max_bubble_w;
    int pad;
    int line_count;
    int k;
    int widest;
    int bubble_w;
    int bubble_x;
    int bubble_h;
    int header_h;
    int fill_pen;
    int text_pen;
    int time_pen;
    int has_time;
    int time_w;
    int has_reply;
    int reply_h;
    const char *read_mark;
    int read_mark_w;
    int has_mark;
    int has_status;
    int status_w;

    pad = 8;
    max_bubble_w = (area_w * 78) / 100;
    if (max_bubble_w < 40) {
        /* Narrow window: keep a right gutter rather than touching the edge. */
        max_bubble_w = (area_w > 2 * pad) ? (area_w - 2 * pad) : area_w;
    }
    line_count = tg_gui_wrap(backend, message->text, max_bubble_w - (2 * pad),
                             starts, lengths, TG_GUI_WRAP_MAX_LINES);
    if (line_count <= 0) {
        line_count = 1;
        starts[0] = 0UL;
        lengths[0] = 0UL;
    }

    widest = 0;
    for (k = 0; k < line_count; ++k) {
        int w;

        w = backend->text_width(backend, message->text + starts[k], lengths[k]);
        if (w > widest) {
            widest = w;
        }
    }
    has_time = (message->time[0] != '\0');
    time_w = has_time ? backend->text_width(backend, message->time,
                                            (unsigned long)strlen(message->time))
                      : 0;
    /* The bottom line carries the timestamp and, for own messages, the read
       receipt mark to its right (a 4px gap between them when both show). */
    read_mark = tg_gui_read_mark(message);
    has_mark = (read_mark[0] != '\0');
    read_mark_w = has_mark ? backend->text_width(backend, read_mark,
                                                 (unsigned long)strlen(read_mark))
                           : 0;
    has_status = has_time || has_mark;
    status_w = time_w + read_mark_w + ((has_time && has_mark) ? 4 : 0);
    if (status_w > widest) {
        widest = status_w; /* the bubble must hold the timestamp/receipt line */
    }
    has_reply = (message->reply_text[0] != '\0');
    reply_h = has_reply ? lh : 0;
    if (has_reply) {
        int reply_w;

        reply_w = backend->text_width(backend, "> ", 2UL) +
                  backend->text_width(
                      backend, message->reply_text,
                      (unsigned long)strlen(message->reply_text));
        if (reply_w > widest) {
            widest = reply_w; /* the quoted reference line must fit too */
        }
    }
    bubble_w = widest + (2 * pad);
    if (bubble_w > max_bubble_w) {
        bubble_w = max_bubble_w;
    }

    /* lh + lh/2: leave a gap below the sender-name baseline (drawn at y+lh) wider
       than the font descent, so the name's descenders (g/j/p/q/y) are not covered
       by the message's coloured background fill. Plain lh+2 left only 2px. */
    header_h = message->is_own ? 0 : (lh + (lh / 2));
    /* Reserve a line inside the bubble for the timestamp / read-receipt mark so
       it stays on the coloured background instead of spilling out below it, plus
       one for the quoted-reply reference line when present. */
    bubble_h = header_h + reply_h + (line_count * lh) + (has_status ? lh : 0) + 6;

    if (message->is_own) {
        bubble_x = area_x + area_w - bubble_w;
        fill_pen = TG_GUI_PEN_ACCENT;
        text_pen = TG_GUI_PEN_ACCENT_TEXT;
        time_pen = TG_GUI_PEN_ACCENT_TEXT;
    } else {
        bubble_x = area_x;
        fill_pen = TG_GUI_PEN_SURFACE;
        text_pen = TG_GUI_PEN_TEXT;
        time_pen = TG_GUI_PEN_TEXT_DIM;
        {
            int sender_pen;

            /* Enforce the avatar-tint band here rather than leaning on the
               backend to clamp an out-of-range sender_color. */
            sender_pen = message->sender_color;
            if (sender_pen < 0 || sender_pen >= TG_GUI_AVATAR_COLORS) {
                sender_pen = 0;
            }
            if ((y + lh) <= bottom) {
                backend->draw_text(backend, sender_pen + TG_GUI_PEN_COUNT,
                                   bubble_x + 2, y + lh, message->sender,
                                   (unsigned long)strlen(message->sender));
            }
        }
    }

    /* Clip every drawn part to the transcript bottom so a tall bubble near the
       edge cannot bleed into the input row / status bar on a resized window. */
    {
        int fill_top;
        int fill_bottom;

        fill_top = y + header_h;
        fill_bottom = y + bubble_h;
        if (fill_bottom > bottom) {
            fill_bottom = bottom;
        }
        if (fill_bottom > fill_top) {
            backend->fill_rect(backend, fill_pen,
                               tg_gui_make_rect(bubble_x, fill_top, bubble_w,
                                                fill_bottom - fill_top));
        }
    }
    if (has_reply) {
        int reply_baseline;

        reply_baseline = y + header_h + lh;
        if (reply_baseline <= bottom) {
            char line[TG_GUI_REPLY_MAX + 4];

            line[0] = '>';
            line[1] = ' ';
            tg_gui_copy(line + 2, sizeof(line) - 2, message->reply_text);
            /* Dimmed, plain (no markup), clipped to the bubble width with "..". */
            tg_gui_draw_clipped(backend, time_pen, bubble_x + pad, reply_baseline,
                                line, bubble_w - (2 * pad));
        }
    }
    style = 0;
    for (k = 0; k < line_count; ++k) {
        int baseline;

        baseline = y + header_h + reply_h + (k * lh) + lh;
        if (baseline <= bottom) {
            tg_gui_draw_markup(backend, text_pen, bubble_x + pad, baseline,
                               message->text + starts[k], lengths[k], &style);
        }
    }
    if (backend->set_style != 0) {
        backend->set_style(backend, 0); /* reset before time / next bubble */
    }
    if (has_status) {
        int status_baseline;
        int status_x;

        /* One line below the last text line, right-aligned, inside the fill:
           timestamp then (for own messages) the read-receipt mark. */
        status_baseline = y + header_h + reply_h + (line_count * lh) + lh;
        status_x = bubble_x + bubble_w - pad - status_w;
        if (status_x < bubble_x + pad) {
            status_x = bubble_x + pad;
        }
        if (status_baseline <= bottom) {
            if (has_time) {
                backend->draw_text(backend, time_pen, status_x, status_baseline,
                                   message->time,
                                   (unsigned long)strlen(message->time));
            }
            if (has_mark) {
                int mark_x;

                mark_x = status_x + time_w + ((has_time) ? 4 : 0);
                backend->draw_text(backend, time_pen, mark_x, status_baseline,
                                   read_mark,
                                   (unsigned long)strlen(read_mark));
            }
        }
    }
    return bubble_h + 6;
}

/* The vertical space a message occupies (matching what tg_gui_paint_bubble
   advances y by) without drawing -- used to anchor the newest messages to the
   bottom of the transcript so a fresh send/receive is always visible. */
static int tg_gui_message_height(tg_gui_backend *backend,
                                 const tg_gui_message *message, int area_w,
                                 int lh)
{
    unsigned long starts[TG_GUI_WRAP_MAX_LINES];
    unsigned long lengths[TG_GUI_WRAP_MAX_LINES];
    int max_bubble_w;
    int pad;
    int line_count;
    int header_h;
    int has_time;
    int has_status;
    int reply_h;

    if (message->is_system) {
        return lh + 6;
    }
    pad = 8;
    max_bubble_w = (area_w * 78) / 100;
    if (max_bubble_w < 40) {
        max_bubble_w = (area_w > 2 * pad) ? (area_w - 2 * pad) : area_w;
    }
    line_count = tg_gui_wrap(backend, message->text, max_bubble_w - (2 * pad),
                             starts, lengths, TG_GUI_WRAP_MAX_LINES);
    if (line_count <= 0) {
        line_count = 1;
    }
    /* lh + lh/2: leave a gap below the sender-name baseline (drawn at y+lh) wider
       than the font descent, so the name's descenders (g/j/p/q/y) are not covered
       by the message's coloured background fill. Plain lh+2 left only 2px. */
    header_h = message->is_own ? 0 : (lh + (lh / 2));
    has_time = (message->time[0] != '\0');
    /* The status line also shows for an own message's read-receipt mark even
       when it has no timestamp (the optimistic echo). */
    has_status = has_time || (tg_gui_read_mark(message)[0] != '\0');
    reply_h = (message->reply_text[0] != '\0') ? lh : 0;
    /* bubble_h + 6, mirroring tg_gui_paint_bubble's return (incl. reply line). */
    return header_h + reply_h + (line_count * lh) + (has_status ? lh : 0) + 6 + 6;
}

/* Draws just the bottom composer row: the input box, the typed text (or the
   placeholder / idle text) with the blinking caret, and the Send button.
   Factored out of tg_gui_paint_main so the caret blink (tg_gui_paint_caret) can
   repaint ONLY this strip instead of the whole window -- a full repaint twice a
   second was visible as a constant refresh on slow planar displays (OS3). The
   geometry is recomputed from the backend so it matches tg_gui_paint_main. */
static void tg_gui_paint_input_row(const tg_gui_state *state,
                                   tg_gui_backend *backend)
{
    int width;
    int height;
    int lh;
    int sidebar_w;
    int status_h;
    int content_h;
    int area_x;
    int input_h;
    int input_baseline;

    width = backend->width(backend);
    height = backend->height(backend);
    lh = backend->line_height(backend);
    if (width <= 0 || height <= 0 || lh <= 0) {
        return;
    }
    status_h = lh + 6;
    content_h = height - status_h;
    sidebar_w = tg_gui_sidebar_w(width);
    area_x = sidebar_w + 12;
    input_h = lh + 14;
    /* Vertically centre the text line inside the input/Send box (height
       input_h - 4 = lh + 10). The optical centre sits a couple of pixels below
       box_top + lh once the Amiga bitmap-font descent (~2px) is accounted for. */
    input_baseline = content_h - input_h + lh + 2;

    backend->fill_rect(backend, TG_GUI_PEN_SURFACE,
                       tg_gui_make_rect(sidebar_w + 8, content_h - input_h,
                                        width - sidebar_w - 16, input_h - 4));
    if (state->composing) {
        int caret_x;

        caret_x = area_x;
        if (state->input[0] != '\0') {
            unsigned long caret_off;

            backend->draw_text(backend, TG_GUI_PEN_TEXT, area_x, input_baseline,
                               state->input,
                               (unsigned long)strlen(state->input));
            /* Caret after the prefix up to input_caret (not the end), so LEFT/
               RIGHT move it within the typed text. */
            caret_off = (unsigned long)state->input_caret;
            if (caret_off > (unsigned long)strlen(state->input)) {
                caret_off = (unsigned long)strlen(state->input);
            }
            caret_x = area_x + backend->text_width(backend, state->input,
                                                   caret_off) +
                      1;
        }
        /* A caret bar at the cursor position; the window toggles cursor_on so
           it blinks while composing. */
        if (state->cursor_on) {
            backend->fill_rect(backend, TG_GUI_PEN_TEXT,
                               tg_gui_make_rect(caret_x, input_baseline - lh + 1,
                                                2, lh));
        }
    } else if (state->input[0] != '\0') {
        backend->draw_text(backend, TG_GUI_PEN_TEXT, area_x,
                           input_baseline, state->input,
                           (unsigned long)strlen(state->input));
    } else {
        backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM, area_x,
                           input_baseline, "Write a message...",
                           18UL);
    }
    backend->fill_rect(backend, TG_GUI_PEN_ACCENT,
                       tg_gui_make_rect(width - 64, content_h - input_h, 56,
                                        input_h - 4));
    backend->draw_text(backend, TG_GUI_PEN_ACCENT_TEXT, width - 56,
                       input_baseline, "Send", 4UL);
}

static void tg_gui_paint_main(const tg_gui_state *state,
                              tg_gui_backend *backend, int sidebar_w,
                              int width, int content_h, int lh)
{
    int area_x;
    int area_w;
    int header_h;
    int input_h;
    int y;
    int i;
    int transcript_bottom;
    int last;
    int transcript_top;
    tg_gui_state *st;

    /* Clear this panel's own background (the sidebar already does the same), so
       tg_gui_paint no longer needs a leading full-window clear that flashed the
       entire window on every repaint -- very visible on OS3 planar displays. The
       sidebar + this main panel + the status bar tile the whole window. */
    backend->fill_rect(backend, TG_GUI_PEN_WINDOW,
                       tg_gui_make_rect(sidebar_w, 0, width - sidebar_w,
                                        content_h));

    area_x = sidebar_w + 12;
    area_w = width - sidebar_w - 24 - TG_GUI_SCROLLBAR_W;
    if (area_w < 40) {
        area_w = 40;
    }

    header_h = lh + 10;
    tg_gui_draw_clipped(backend, TG_GUI_PEN_TEXT, area_x, lh + 2, state->title,
                        area_w);
    /* While the peer is typing, the second header line shows "X is typing..."
       in the accent colour instead of the static subtitle (Telegram's cue). */
    if (state->typing[0] != '\0') {
        tg_gui_draw_clipped(backend, TG_GUI_PEN_ACCENT, area_x, header_h + lh - 2,
                            state->typing, area_w);
    } else {
        tg_gui_draw_clipped(backend, TG_GUI_PEN_TEXT_DIM, area_x,
                            header_h + lh - 2, state->subtitle, area_w);
    }

    input_h = lh + 14;
    transcript_bottom = content_h - input_h - 4;

    /* One full line below the subtitle baseline so the first incoming bubble's
       sender name clears the header at any font size. */
    y = header_h + (2 * lh) + 4;
    transcript_top = y;
    /* Walk back from the newest VISIBLE message (message_count minus the rows
       scrolled up past), summing heights until the visible set fills the
       transcript, then anchor that set to the bottom. transcript_scroll == 0
       keeps the newest pinned to the bottom (the live default). */
    {
        int avail;
        int used;
        int max_scroll;
        int view_msgs;

        avail = transcript_bottom - transcript_top;
        /* Max scroll: how many newest messages can be hidden before the oldest
           fills the top of a full view (forward-walk from the oldest). */
        {
            int fwd_used = 0;
            int top_fit = 0;
            int j = 0;

            while (j < state->message_count) {
                int h = tg_gui_message_height(backend, &state->messages[j],
                                              area_w, lh);
                if (fwd_used > 0 && fwd_used + h > avail) {
                    break;
                }
                fwd_used += h;
                ++top_fit;
                ++j;
            }
            max_scroll = state->message_count - top_fit;
            if (max_scroll < 0) {
                max_scroll = 0;
            }
        }
        /* The painter owns the geometry: clamp the offset the event loop
           advanced freely (cast away const to write the model's own field). */
        st = (tg_gui_state *)state;
        if (st->transcript_scroll > max_scroll) {
            st->transcript_scroll = max_scroll;
        }
        if (st->transcript_scroll < 0) {
            st->transcript_scroll = 0;
        }
        last = state->message_count - state->transcript_scroll;

        used = 0;
        i = last;
        while (i > 0) {
            int h;

            h = tg_gui_message_height(backend, &state->messages[i - 1], area_w,
                                      lh);
            if (used > 0 && used + h > avail) {
                break;
            }
            used += h;
            --i;
        }
        if (used < avail) {
            y = transcript_bottom - used;
        }
        view_msgs = last - i;
        if (view_msgs < 1) {
            view_msgs = 1;
        }
        if (max_scroll > 0) {
            int ky;
            int kh;

            tg_gui_paint_scrollbar(
                backend, width - TG_GUI_SCROLLBAR_W, transcript_top,
                transcript_bottom - transcript_top, state->message_count,
                view_msgs,
                (state->message_count - view_msgs) - state->transcript_scroll,
                &ky, &kh);
            st->sb_tr_x = width - TG_GUI_SCROLLBAR_W;
            st->sb_tr_ty = transcript_top;
            st->sb_tr_th = transcript_bottom - transcript_top;
            st->sb_tr_ky = ky;
            st->sb_tr_kh = kh;
            st->sb_tr_max = state->message_count - view_msgs;
        } else {
            st->sb_tr_max = 0;
        }
    }
    for (; i < last; ++i) {
        const tg_gui_message *message;

        /* Guard every message type, system lines included, against running
           into the input row. */
        if (y >= transcript_bottom) {
            break;
        }
        message = &state->messages[i];
        if (message->is_system) {
            if (y + lh <= transcript_bottom) {
                backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM,
                                   area_x + (area_w / 4), y + lh, message->text,
                                   (unsigned long)strlen(message->text));
            }
            y += lh + 6;
            continue;
        }
        y += tg_gui_paint_bubble(backend, message, area_x, area_w, y, lh,
                                 transcript_bottom);
    }

    tg_gui_paint_input_row(state, backend);
}

/* Sidebar width for a given window width -- shared by the painter and the
   hit-test so a click maps to exactly what was drawn. */
int tg_gui_sidebar_w(int width)
{
    int sidebar_w;

    if (width < 280) {
        sidebar_w = width / 3;
    } else {
        sidebar_w = (width * 36) / 100;
        if (sidebar_w < 120) {
            sidebar_w = 120;
        }
        if (sidebar_w > width - 160) {
            sidebar_w = width - 160;
        }
    }
    if (sidebar_w < 1) {
        sidebar_w = 1;
    }
    return sidebar_w;
}

int tg_gui_hit_test(const tg_gui_state *state, int width, int height, int lh,
                    int x, int y)
{
    int sidebar_w;
    int status_h;
    int content_h;
    int input_h;

    if (state == 0 || lh <= 0 || width <= 0 || height <= 0) {
        return TG_GUI_HIT_NONE;
    }
    status_h = lh + 6;
    content_h = height - status_h;
    sidebar_w = tg_gui_sidebar_w(width);
    input_h = lh + 14;
    /* The input row / Send button live along the bottom of the right pane. */
    if (y >= content_h - input_h && y < content_h - 4 && x >= sidebar_w) {
        if (x >= width - 64) {
            return TG_GUI_HIT_SEND;
        }
        return TG_GUI_HIT_INPUT;
    }
    /* Sidebar chat rows (search box on top, then fixed-height rows). */
    if (x >= 0 && x < sidebar_w && y >= 0 && y < content_h) {
        int search_h;
        int row_h;

        search_h = lh + 10;
        row_h = (2 * lh) + 12;
        if (y >= search_h) {
            int row;

            row = (y - search_h) / row_h + state->chat_scroll;
            if (row >= 0 && row < state->chat_count) {
                return row;
            }
        }
    }
    return TG_GUI_HIT_NONE;
}

/* Draws one centered line, clamped into the window so it never overflows. */
static void tg_gui_draw_centered(tg_gui_backend *backend, int pen, int width,
                                 int baseline, const char *text)
{
    int tw;
    int x;

    if (text == 0 || text[0] == '\0' || baseline < 0) {
        return;
    }
    tw = backend->text_width(backend, text, (unsigned long)strlen(text));
    x = (width - tw) / 2;
    if (x < 2) {
        x = 2;
    }
    tg_gui_draw_clipped(backend, pen, x, baseline, text, width - x - 2);
}

/* Draws just the login input box: the SURFACE field, the typed text (masked for
   the 2FA password) centred in the box, and the blinking caret. Factored out of
   tg_gui_paint_login so the caret blink (tg_gui_paint_caret) repaints ONLY this
   box, not the whole login screen. Geometry is recomputed to match
   tg_gui_paint_login. */
static void tg_gui_paint_login_input(const tg_gui_state *state,
                                     tg_gui_backend *backend)
{
    int width;
    int height;
    int lh;
    int cx;
    int mid;
    int box_w;
    int box_x;
    int box_y;
    int box_h;
    const char *field;
    char masked[TG_GUI_TEXT_MAX];

    width = backend->width(backend);
    height = backend->height(backend);
    lh = backend->line_height(backend);
    if (width <= 0 || height <= 0 || lh <= 0) {
        return;
    }
    cx = width / 2;
    mid = height / 2;

    box_w = (width - 40 < 280) ? (width - 40) : 280;
    if (box_w < 40) {
        box_w = (width > 8) ? (width - 8) : width;
    }
    box_x = cx - (box_w / 2);
    box_y = mid + 4;
    box_h = lh + 8;
    backend->fill_rect(backend, TG_GUI_PEN_SURFACE,
                       tg_gui_make_rect(box_x, box_y, box_w, box_h));

    if (state->input_masked) {
        unsigned long n;
        unsigned long i;

        n = (unsigned long)strlen(state->input);
        if (n >= sizeof(masked)) {
            n = sizeof(masked) - 1UL;
        }
        for (i = 0UL; i < n; ++i) {
            masked[i] = '*';
        }
        masked[n] = '\0';
        field = masked;
    } else {
        field = state->input;
    }
    {
        int tw;
        int text_x;

        /* Centre the typed text in the box so it lines up under the centred
           title/status above and the hint below. Long input falls back to
           left-aligned + clipped to the box. */
        tw = backend->text_width(backend, field, (unsigned long)strlen(field));
        text_x = box_x + (box_w - tw) / 2;
        if (text_x < box_x + 6) {
            text_x = box_x + 6;
        }
        tg_gui_draw_clipped(backend, TG_GUI_PEN_TEXT, text_x, box_y + lh + 1,
                            field, box_x + box_w - 6 - text_x);
        if (state->cursor_on) {
            int caret_x;

            caret_x = text_x + tw;
            if (caret_x < box_x + box_w - 2) {
                backend->fill_rect(backend, TG_GUI_PEN_ACCENT,
                                   tg_gui_make_rect(caret_x, box_y + 3, 2,
                                                    box_h - 6));
            }
        }
    }
}

/* The first-login screen: a centered panel with the title, the current prompt
   (state->status), the input field (masked for the 2FA password) with caret,
   and a key hint. Shown while state->mode is a TG_GUI_MODE_LOGIN_* value. */
static void tg_gui_paint_login(const tg_gui_state *state,
                               tg_gui_backend *backend)
{
    int width;
    int height;
    int lh;
    int mid;

    width = backend->width(backend);
    height = backend->height(backend);
    lh = backend->line_height(backend);
    if (width <= 0 || height <= 0 || lh <= 0) {
        return;
    }
    if (tg_gui_clear_background) {
        backend->fill_rect(backend, TG_GUI_PEN_WINDOW,
                           tg_gui_make_rect(0, 0, width, height));
    }
    mid = height / 2;

    tg_gui_draw_centered(backend, TG_GUI_PEN_ACCENT, width, mid - (3 * lh),
                         "Telegram Amiga");
    tg_gui_draw_centered(backend, TG_GUI_PEN_TEXT, width, mid - lh,
                         state->status);

    tg_gui_paint_login_input(state, backend);

    tg_gui_draw_centered(backend, TG_GUI_PEN_TEXT_DIM, width, mid + (3 * lh),
                         "ENTER confirms   ESC quits");
}

/* Repaints ONLY the active caret region -- the composer input row in chat mode,
   or the login input box otherwise -- so the ~2 Hz caret blink no longer
   repaints the whole window (a visible, constant refresh on slow OS3 displays).
   Geometry is recomputed by the panel helpers to match a full tg_gui_paint. */
void tg_gui_paint_caret(const tg_gui_state *state, tg_gui_backend *backend)
{
    if (state == 0 || backend == 0) {
        return;
    }
    if (state->mode != TG_GUI_MODE_CHAT) {
        tg_gui_paint_login_input(state, backend);
    } else {
        tg_gui_paint_input_row(state, backend);
    }
}

void tg_gui_paint(const tg_gui_state *state, tg_gui_backend *backend)
{
    int width;
    int height;
    int lh;
    int sidebar_w;
    int status_h;
    int content_h;

    if (state == 0 || backend == 0) {
        return;
    }
    if (state->mode != TG_GUI_MODE_CHAT) {
        tg_gui_paint_login(state, backend);
        return;
    }
    width = backend->width(backend);
    height = backend->height(backend);
    lh = backend->line_height(backend);
    if (width <= 0 || height <= 0 || lh <= 0) {
        return;
    }

    status_h = lh + 6;
    content_h = height - status_h;
    /* Below ~280px there is no room for both a 120px list and a usable
       conversation, so just split a third; above it, 36% clamped to [120,
       width-160]. */
    sidebar_w = tg_gui_sidebar_w(width);

    /* No leading full-window clear: tg_gui_paint_sidebar, tg_gui_paint_main and
       the status bar below each fill their own region, so they tile the whole
       window. This avoids the full-window flash that was visible as a constant
       refresh on slow OS3 planar displays. */
    tg_gui_paint_sidebar(state, backend, sidebar_w, content_h, lh);
    tg_gui_paint_main(state, backend, sidebar_w, width, content_h, lh);

    backend->fill_rect(backend, TG_GUI_PEN_SURFACE,
                       tg_gui_make_rect(0, content_h, width, status_h));
    backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM, 10, content_h + lh,
                       state->status, (unsigned long)strlen(state->status));
}

/* --- Recording backend for the self-test ------------------------------- */

typedef struct tg_gui_record {
    int width;
    int height;
    int fills;
    int avatars;
    int texts;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int read_marks; /* draw_text calls that emitted a "v"/"vv" receipt mark */
    const char *forbidden; /* a string that must NEVER be drawn (e.g. a password) */
    int forbidden_hits;    /* how many draw_text calls contained `forbidden` */
} tg_gui_record;

static int tg_gui_rec_width(tg_gui_backend *backend)
{
    return ((tg_gui_record *)backend->context)->width;
}

static int tg_gui_rec_height(tg_gui_backend *backend)
{
    return ((tg_gui_record *)backend->context)->height;
}

static int tg_gui_rec_line_height(tg_gui_backend *backend)
{
    (void)backend;
    return 10;
}

static int tg_gui_rec_text_width(tg_gui_backend *backend, const char *text,
                                 unsigned long length)
{
    (void)backend;
    (void)text;
    return (int)(length * 6UL);
}

static void tg_gui_rec_track(tg_gui_record *record, int x, int y)
{
    if (x < record->min_x) {
        record->min_x = x;
    }
    if (y < record->min_y) {
        record->min_y = y;
    }
    if (x > record->max_x) {
        record->max_x = x;
    }
    if (y > record->max_y) {
        record->max_y = y;
    }
}

static void tg_gui_rec_fill(tg_gui_backend *backend, int pen, tg_gui_rect rect)
{
    tg_gui_record *record;

    (void)pen;
    record = (tg_gui_record *)backend->context;
    record->fills += 1;
    tg_gui_rec_track(record, rect.x, rect.y);
    tg_gui_rec_track(record, rect.x + rect.w, rect.y + rect.h);
}

static void tg_gui_rec_avatar(tg_gui_backend *backend, int color_index,
                              tg_gui_rect rect)
{
    tg_gui_record *record;

    (void)color_index;
    record = (tg_gui_record *)backend->context;
    record->avatars += 1;
    tg_gui_rec_track(record, rect.x, rect.y);
    tg_gui_rec_track(record, rect.x + rect.w, rect.y + rect.h);
}

static void tg_gui_rec_text(tg_gui_backend *backend, int pen, int x,
                            int baseline, const char *text,
                            unsigned long length)
{
    tg_gui_record *record;

    (void)pen;
    record = (tg_gui_record *)backend->context;
    record->texts += 1;
    /* A bare "v" / "vv" run is the read-receipt mark (no other demo string is
       just v's), so count it to prove the mark reaches the backend. */
    if (text != 0 && length >= 1UL && length <= 2UL) {
        unsigned long n;

        for (n = 0UL; n < length && text[n] == 'v'; ++n) {
        }
        if (n == length) {
            record->read_marks += 1;
        }
    }
    /* Guard that a forbidden string (a masked password) never reaches draw. */
    if (record->forbidden != 0 && record->forbidden[0] != '\0' && text != 0) {
        unsigned long flen;

        flen = (unsigned long)strlen(record->forbidden);
        if (length >= flen) {
            unsigned long i;

            for (i = 0UL; i + flen <= length; ++i) {
                unsigned long j;

                for (j = 0UL; j < flen && text[i + j] == record->forbidden[j];
                     ++j) {
                }
                if (j == flen) {
                    record->forbidden_hits += 1;
                    break;
                }
            }
        }
    }
    tg_gui_rec_track(record, x, baseline);
    tg_gui_rec_track(record, x + (int)(length * 6UL), baseline);
}

int tg_gui_self_test(void)
{
    tg_gui_state state;
    tg_gui_backend backend;
    tg_gui_record record;
    int ok;

    tg_gui_demo_state(&state);
    if (state.chat_count <= 0 || state.message_count <= 0) {
        puts("gui self-test: demo state empty");
        return 2;
    }

    memset(&record, 0, sizeof(record));
    record.width = 480;
    record.height = 320;
    record.min_x = record.width;
    record.min_y = record.height;
    record.max_x = 0;
    record.max_y = 0;

    backend.context = &record;
    backend.width = tg_gui_rec_width;
    backend.height = tg_gui_rec_height;
    backend.line_height = tg_gui_rec_line_height;
    backend.text_width = tg_gui_rec_text_width;
    backend.fill_rect = tg_gui_rec_fill;
    backend.avatar_fill = tg_gui_rec_avatar;
    backend.draw_text = tg_gui_rec_text;
    backend.set_style = 0; /* recorder renders plain; markers are just skipped */

    tg_gui_paint(&state, &backend);

    ok = 1;
    if (record.fills <= 0 || record.texts <= 0 || record.avatars <= 0) {
        ok = 0;
    }
    if (record.min_x < 0 || record.min_y < 0) {
        ok = 0;
    }
    if (record.max_x > record.width || record.max_y > record.height) {
        ok = 0;
    }
    /* The demo's one "seen" own message must emit exactly one receipt mark. */
    if (record.read_marks != 1) {
        printf("gui self-test: expected 1 read mark, drew %d\n",
               record.read_marks);
        return 2;
    }
    if (!ok) {
        printf("gui self-test: failed (%d fills, %d avatars, %d texts, "
               "bounds x[%d..%d] y[%d..%d] in %dx%d)\n",
               record.fills, record.avatars, record.texts, record.min_x,
               record.max_x, record.min_y, record.max_y, record.width,
               record.height);
        return 2;
    }

    /* The "is typing" header line is a transient overlay (live-only, not in the
       demo); paint it once into a fresh recorder to keep that branch in bounds. */
    {
        tg_gui_record trec;

        tg_gui_copy(state.typing, sizeof(state.typing), "Mario is typing...");
        memset(&trec, 0, sizeof(trec));
        trec.width = 480;
        trec.height = 320;
        trec.min_x = trec.width;
        trec.min_y = trec.height;
        backend.context = &trec;
        tg_gui_paint(&state, &backend);
        state.typing[0] = '\0';
        if (trec.texts <= 0 || trec.min_x < 0 || trec.min_y < 0 ||
            trec.max_x > trec.width || trec.max_y > trec.height) {
            puts("gui self-test: typing header overlay out of bounds");
            return 2;
        }
    }

    /* The first-login 2FA screen must paint in bounds AND mask the password:
       the raw secret must never reach the backend's draw_text. */
    {
        tg_gui_state ls;
        tg_gui_record lrec;

        memset(&ls, 0, sizeof(ls));
        ls.theme = TG_GUI_THEME_DARK;
        ls.mode = TG_GUI_MODE_LOGIN_2FA;
        ls.input_masked = 1;
        ls.cursor_on = 1;
        tg_gui_copy(ls.input, sizeof(ls.input), "zzqp7secret");
        tg_gui_copy(ls.status, sizeof(ls.status), "2FA password");

        memset(&lrec, 0, sizeof(lrec));
        lrec.width = 480;
        lrec.height = 320;
        lrec.min_x = lrec.width;
        lrec.min_y = lrec.height;
        lrec.forbidden = "zzqp7secret";
        backend.context = &lrec;
        tg_gui_paint(&ls, &backend);
        if (lrec.texts <= 0 || lrec.min_x < 0 || lrec.min_y < 0 ||
            lrec.max_x > lrec.width || lrec.max_y > lrec.height) {
            puts("gui self-test: login screen out of bounds");
            return 2;
        }
        if (lrec.forbidden_hits != 0) {
            puts("gui self-test: 2FA password must be masked, not drawn");
            return 2;
        }
    }

    printf("gui self-test: ok (%d chats, %d msgs, %d fills, %d avatars, "
           "%d texts, within %dx%d)\n",
           state.chat_count, state.message_count, record.fills, record.avatars,
           record.texts, record.width, record.height);
    return 0;
}
