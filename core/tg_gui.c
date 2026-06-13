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
    tg_gui_copy(state->subtitle, sizeof(state->subtitle), "gruppo - 128 membri");

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
    state->message_count = 4;

    tg_gui_copy(state->input, sizeof(state->input), "");
    tg_gui_copy(state->status, sizeof(state->status), "Connesso - DC4");
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

static void tg_gui_paint_sidebar(const tg_gui_state *state,
                                 tg_gui_backend *backend, int sidebar_w,
                                 int content_h, int lh)
{
    int search_h;
    int row_h;
    int y;
    int i;

    backend->fill_rect(backend, TG_GUI_PEN_SURFACE,
                       tg_gui_make_rect(0, 0, sidebar_w, content_h));

    search_h = lh + 10;
    backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM, 10, (search_h / 2) + 4,
                       "Cerca chat...", 13UL);

    row_h = (2 * lh) + 12;
    y = search_h;
    for (i = 0; i < state->chat_count && y + row_h <= content_h; ++i) {
        const tg_gui_chat *chat;
        int avatar;
        int text_x;
        int name_pen;
        int preview_pen;

        chat = &state->chats[i];
        if (i == state->selected_chat) {
            backend->fill_rect(backend, TG_GUI_PEN_SELECT,
                               tg_gui_make_rect(0, y, sidebar_w, row_h));
            backend->fill_rect(backend, TG_GUI_PEN_ACCENT,
                               tg_gui_make_rect(0, y, 3, row_h));
            name_pen = TG_GUI_PEN_TEXT;
            preview_pen = TG_GUI_PEN_TEXT_DIM;
        } else {
            name_pen = TG_GUI_PEN_TEXT;
            preview_pen = TG_GUI_PEN_TEXT_DIM;
        }

        avatar = (2 * lh);
        backend->avatar_fill(backend, chat->avatar_color,
                             tg_gui_make_rect(8, y + 6, avatar, avatar));
        backend->draw_text(backend, TG_GUI_PEN_TEXT, 8 + 6, y + 6 + lh,
                           chat->initials, (unsigned long)strlen(chat->initials));

        text_x = 8 + avatar + 8;
        backend->draw_text(backend, name_pen, text_x, y + 4 + lh, chat->name,
                           (unsigned long)strlen(chat->name));
        backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM,
                           sidebar_w - 34, y + 4 + lh, chat->time,
                           (unsigned long)strlen(chat->time));
        backend->draw_text(backend, preview_pen, text_x, y + 8 + (2 * lh),
                           chat->preview, (unsigned long)strlen(chat->preview));
        if (chat->unread > 0) {
            char badge[8];

            sprintf(badge, "%d", chat->unread > 999 ? 999 : chat->unread);
            backend->fill_rect(backend, TG_GUI_PEN_BADGE,
                               tg_gui_make_rect(sidebar_w - 28,
                                                y + 6 + (2 * lh) - lh, 20,
                                                lh + 2));
            backend->draw_text(backend, TG_GUI_PEN_BADGE_TEXT, sidebar_w - 22,
                               y + 8 + (2 * lh), badge,
                               (unsigned long)strlen(badge));
        }
        y += row_h;
    }
}

static int tg_gui_paint_bubble(tg_gui_backend *backend,
                               const tg_gui_message *message, int area_x,
                               int area_w, int y, int lh)
{
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

    pad = 8;
    max_bubble_w = (area_w * 78) / 100;
    if (max_bubble_w < 40) {
        max_bubble_w = area_w;
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
    bubble_w = widest + (2 * pad);
    if (bubble_w > max_bubble_w) {
        bubble_w = max_bubble_w;
    }

    header_h = message->is_own ? 0 : (lh + 2);
    bubble_h = header_h + (line_count * lh) + pad;

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
        backend->draw_text(backend, message->sender_color + TG_GUI_PEN_COUNT,
                           bubble_x + 2, y + lh, message->sender,
                           (unsigned long)strlen(message->sender));
    }

    backend->fill_rect(backend, fill_pen,
                       tg_gui_make_rect(bubble_x, y + header_h, bubble_w,
                                        bubble_h - header_h));
    for (k = 0; k < line_count; ++k) {
        backend->draw_text(backend, text_pen, bubble_x + pad,
                           y + header_h + (k * lh) + lh,
                           message->text + starts[k], lengths[k]);
    }
    if (message->time[0] != '\0') {
        backend->draw_text(backend, time_pen, bubble_x + pad,
                           y + bubble_h, message->time,
                           (unsigned long)strlen(message->time));
    }
    return bubble_h + 6;
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

    area_x = sidebar_w + 12;
    area_w = width - sidebar_w - 24;
    if (area_w < 40) {
        area_w = 40;
    }

    header_h = lh + 10;
    backend->draw_text(backend, TG_GUI_PEN_TEXT, area_x, lh + 2, state->title,
                       (unsigned long)strlen(state->title));
    backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM, area_x, header_h + lh - 2,
                       state->subtitle, (unsigned long)strlen(state->subtitle));

    input_h = lh + 14;
    transcript_bottom = content_h - input_h - 4;

    y = header_h + lh + 6;
    for (i = 0; i < state->message_count; ++i) {
        const tg_gui_message *message;

        message = &state->messages[i];
        if (message->is_system) {
            backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM, area_x + (area_w / 4),
                               y + lh, message->text,
                               (unsigned long)strlen(message->text));
            y += lh + 6;
            continue;
        }
        if (y >= transcript_bottom) {
            break;
        }
        y += tg_gui_paint_bubble(backend, message, area_x, area_w, y, lh);
    }

    backend->fill_rect(backend, TG_GUI_PEN_SURFACE,
                       tg_gui_make_rect(sidebar_w + 8, content_h - input_h,
                                        width - sidebar_w - 16, input_h - 4));
    if (state->input[0] != '\0') {
        backend->draw_text(backend, TG_GUI_PEN_TEXT, area_x,
                           content_h - input_h + lh, state->input,
                           (unsigned long)strlen(state->input));
    } else {
        backend->draw_text(backend, TG_GUI_PEN_TEXT_DIM, area_x,
                           content_h - input_h + lh, "Scrivi un messaggio...",
                           22UL);
    }
    backend->fill_rect(backend, TG_GUI_PEN_ACCENT,
                       tg_gui_make_rect(width - 64, content_h - input_h, 56,
                                        input_h - 4));
    backend->draw_text(backend, TG_GUI_PEN_ACCENT_TEXT, width - 56,
                       content_h - input_h + lh, "Invia", 5UL);
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
    width = backend->width(backend);
    height = backend->height(backend);
    lh = backend->line_height(backend);
    if (width <= 0 || height <= 0 || lh <= 0) {
        return;
    }

    status_h = lh + 6;
    content_h = height - status_h;
    sidebar_w = (width * 36) / 100;
    if (sidebar_w < 120) {
        sidebar_w = 120;
    }
    if (sidebar_w > width - 160) {
        sidebar_w = width - 160;
    }
    if (sidebar_w < 0) {
        sidebar_w = width / 3;
    }

    backend->fill_rect(backend, TG_GUI_PEN_WINDOW,
                       tg_gui_make_rect(0, 0, width, height));
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
    (void)text;
    record = (tg_gui_record *)backend->context;
    record->texts += 1;
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
    if (!ok) {
        printf("gui self-test: failed (%d fills, %d avatars, %d texts, "
               "bounds x[%d..%d] y[%d..%d] in %dx%d)\n",
               record.fills, record.avatars, record.texts, record.min_x,
               record.max_x, record.min_y, record.max_y, record.width,
               record.height);
        return 2;
    }

    printf("gui self-test: ok (%d chats, %d msgs, %d fills, %d avatars, "
           "%d texts, within %dx%d)\n",
           state.chat_count, state.message_count, record.fills, record.avatars,
           record.texts, record.width, record.height);
    return 0;
}
