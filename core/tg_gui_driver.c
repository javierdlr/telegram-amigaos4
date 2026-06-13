/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * GUI chat driver: projects chat-engine rows into tg_gui_state. See
 * tg_gui_driver.h. The console driver prints a row; this one appends it to the
 * GUI message model.
 */

#include "tg_gui_driver.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void tg_gui_driver_copy(char *dest, unsigned long size, const char *src)
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

int tg_gui_driver_color_for(const char *name)
{
    unsigned long hash;
    unsigned long i;

    if (name == 0 || name[0] == '\0') {
        return 0;
    }
    hash = 5381UL;
    for (i = 0UL; name[i] != '\0'; ++i) {
        hash = ((hash << 5) + hash) + (unsigned long)(unsigned char)name[i];
    }
    return (int)(hash % (unsigned long)TG_GUI_AVATAR_COLORS);
}

static void tg_gui_driver_on_message(void *ctx, const tg_chat_message_row *row)
{
    tg_gui_chat_driver *gui;
    tg_gui_state *state;
    tg_gui_message *message;
    const char *who;

    gui = (tg_gui_chat_driver *)ctx;
    if (gui == 0 || gui->state == 0 || row == 0) {
        return;
    }
    state = gui->state;

    /* Keep the most recent TG_GUI_MAX_MESSAGES: drop the oldest when full. */
    if (state->message_count >= TG_GUI_MAX_MESSAGES) {
        int i;

        for (i = 1; i < TG_GUI_MAX_MESSAGES; ++i) {
            state->messages[i - 1] = state->messages[i];
        }
        state->message_count = TG_GUI_MAX_MESSAGES - 1;
    }
    message = &state->messages[state->message_count];
    memset(message, 0, sizeof(*message));

    message->is_own = row->is_out ? 1 : 0;
    message->is_system = 0;

    /* Sender label: own name when outgoing; the resolved sender otherwise; the
       1:1 peer name as the fallback; empty for an unresolved group author (the
       GUI shows the bubble without a name rather than a "?:" marker). */
    if (row->is_out) {
        who = (row->own_label != 0 && row->own_label[0] != '\0') ? row->own_label
                                                                 : "";
    } else if (row->sender != 0) {
        who = row->sender;
    } else if (!row->is_group && row->peer_label != 0 &&
               row->peer_label[0] != '\0') {
        who = row->peer_label;
    } else {
        who = "";
    }
    tg_gui_driver_copy(message->sender, sizeof(message->sender), who);
    message->sender_color = tg_gui_driver_color_for(who);
    tg_gui_driver_copy(message->text, sizeof(message->text), row->text);

    if (row->has_time) {
        time_t when;
        struct tm *parts;

        when = (time_t)row->local_epoch;
        parts = gmtime(&when);
        if (parts != 0) {
            sprintf(message->time, "%02d:%02d", parts->tm_hour, parts->tm_min);
        }
    }

    state->message_count += 1;
}

/* Derives 1-2 uppercase initials from a display name (skipping a leading '@'):
   first letter of the first word, plus the first letter of the second word if
   present. "Mario Rossi" -> "MR", "Anna" -> "A", "" -> "?". */
static void tg_gui_driver_initials(char *dest, unsigned long size,
                                   const char *name)
{
    unsigned long out;
    unsigned long i;
    int in_word;
    int words;

    if (dest == 0 || size == 0UL) {
        return;
    }
    if (name != 0 && name[0] == '@') {
        ++name;
    }
    out = 0UL;
    in_word = 0;
    words = 0;
    for (i = 0UL; name != 0 && name[i] != '\0' && out + 1UL < size; ++i) {
        char c;

        c = name[i];
        if (c == ' ' || c == '\t') {
            in_word = 0;
            continue;
        }
        if (!in_word) {
            in_word = 1;
            ++words;
            if (words > 2) {
                break;
            }
            if (c >= 'a' && c <= 'z') {
                c = (char)(c - 'a' + 'A');
            }
            dest[out++] = c;
        }
    }
    if (out == 0UL && size > 1UL) {
        dest[out++] = '?';
    }
    dest[out] = '\0';
}

static void tg_gui_driver_on_chat_list_changed(void *ctx,
                                               const tg_chat_list_row *rows,
                                               int count)
{
    tg_gui_chat_driver *gui;
    tg_gui_state *state;
    int i;
    int n;

    gui = (tg_gui_chat_driver *)ctx;
    if (gui == 0 || gui->state == 0 || rows == 0) {
        return;
    }
    state = gui->state;
    n = count;
    if (n > TG_GUI_MAX_CHATS) {
        n = TG_GUI_MAX_CHATS;
    }
    state->chat_count = 0;
    state->selected_chat = 0;
    for (i = 0; i < n; ++i) {
        tg_gui_chat *chat;
        char display[TG_GUI_NAME_MAX];

        chat = &state->chats[state->chat_count];
        memset(chat, 0, sizeof(*chat));
        /* The display name keeps the "@" for username-only chats; the peer
           cache has no last-message preview, so preview/time stay empty. */
        if (rows[i].name_is_username && rows[i].name[0] != '\0') {
            display[0] = '@';
            tg_gui_driver_copy(display + 1, sizeof(display) - 1U, rows[i].name);
        } else {
            tg_gui_driver_copy(display, sizeof(display), rows[i].name);
        }
        tg_gui_driver_copy(chat->name, sizeof(chat->name), display);
        tg_gui_driver_initials(chat->initials, sizeof(chat->initials), display);
        chat->avatar_color = tg_gui_driver_color_for(display);
        chat->unread = (rows[i].unread > 0UL) ? (int)rows[i].unread : 0;
        chat->peer_id_hi = rows[i].peer_id_hi;
        chat->peer_id_lo = rows[i].peer_id_lo;
        chat->flash = 0;
        if (rows[i].is_current) {
            state->selected_chat = state->chat_count;
        }
        state->chat_count += 1;
    }
}

/* A cross-chat notification: find the sidebar row whose peer id matches and
   bump its unread badge + flag it to blink. Peer ids in one cache are unique
   per chat, so the id alone disambiguates. The currently open chat does not
   flash (you are already reading it). Unmatched peers (not in the sidebar) are
   ignored -- there is no row to badge. */
static void tg_gui_driver_on_notification(void *ctx,
                                          const tg_chat_notify_entry *entry)
{
    tg_gui_chat_driver *gui;
    tg_gui_state *state;
    int i;

    gui = (tg_gui_chat_driver *)ctx;
    if (gui == 0 || gui->state == 0 || entry == 0) {
        return;
    }
    state = gui->state;
    for (i = 0; i < state->chat_count; ++i) {
        tg_gui_chat *chat;

        chat = &state->chats[i];
        if (chat->peer_id_hi == entry->peer_id_hi &&
            chat->peer_id_lo == entry->peer_id_lo) {
            chat->unread += 1;
            if (i != state->selected_chat) {
                chat->flash = 1;
            }
            return;
        }
    }
}

void tg_gui_chat_driver_bind(tg_gui_chat_driver *gui, tg_gui_state *state,
                             tg_chat_driver *chat_driver)
{
    if (gui == 0 || chat_driver == 0) {
        return;
    }
    gui->state = state;
    chat_driver->ctx = gui;
    chat_driver->on_message = tg_gui_driver_on_message;
    chat_driver->on_chat_list_changed = tg_gui_driver_on_chat_list_changed;
    chat_driver->on_notification = tg_gui_driver_on_notification;
}

/* --- self-test ---------------------------------------------------------- */

static void tg_gui_driver_emit(tg_chat_driver *driver, unsigned long epoch,
                               int has_time, int is_out, int is_group,
                               const char *peer_label, const char *own_label,
                               const char *sender, const char *text)
{
    tg_chat_message_row row;

    row.text = text;
    row.local_epoch = epoch;
    row.has_time = has_time;
    row.is_out = is_out;
    row.is_group = is_group;
    row.peer_label = peer_label;
    row.own_label = own_label;
    row.sender = sender;
    driver->on_message(driver->ctx, &row);
}

int tg_gui_driver_self_test(void)
{
    tg_gui_state state;
    tg_gui_chat_driver gui;
    tg_chat_driver driver;
    int i;

    memset(&state, 0, sizeof(state));
    tg_gui_chat_driver_bind(&gui, &state, &driver);

    /* Incoming 1:1 (peer fallback), outgoing (own label), group (resolved
       sender), group unknown (no name), and a no-time row. */
    tg_gui_driver_emit(&driver, 1700000000UL, 1, 0, 0, "Mario", "Io", 0, "Ciao");
    tg_gui_driver_emit(&driver, 1700000300UL, 1, 1, 0, "Mario", "Io", 0,
                       "Tutto bene?");
    tg_gui_driver_emit(&driver, 1700003600UL, 1, 0, 1, "Sviluppo", "Io", "Alice",
                       "Salve");
    tg_gui_driver_emit(&driver, 1700003900UL, 1, 0, 1, "Sviluppo", "Io", 0,
                       "boh");
    tg_gui_driver_emit(&driver, 0UL, 0, 0, 0, "Mario", "Io", 0, "senza data");

    if (state.message_count != 5) {
        printf("gui driver self-test: count %d != 5\n", state.message_count);
        return 2;
    }
    if (state.messages[0].is_own != 0 ||
        strcmp(state.messages[0].sender, "Mario") != 0 ||
        strcmp(state.messages[0].text, "Ciao") != 0 ||
        strcmp(state.messages[0].time, "22:13") != 0) {
        puts("gui driver self-test: incoming 1:1 row wrong");
        return 2;
    }
    if (state.messages[1].is_own != 1 ||
        strcmp(state.messages[1].sender, "Io") != 0 ||
        strcmp(state.messages[1].text, "Tutto bene?") != 0) {
        puts("gui driver self-test: outgoing row wrong");
        return 2;
    }
    if (state.messages[2].is_own != 0 ||
        strcmp(state.messages[2].sender, "Alice") != 0 ||
        strcmp(state.messages[2].time, "23:13") != 0) {
        puts("gui driver self-test: group resolved-sender row wrong");
        return 2;
    }
    if (state.messages[3].sender[0] != '\0') {
        puts("gui driver self-test: unknown group author should have no name");
        return 2;
    }
    if (state.messages[4].time[0] != '\0') {
        puts("gui driver self-test: no-date row should have empty time");
        return 2;
    }
    /* Stable, in-range avatar colours; same name -> same colour. */
    if (state.messages[0].sender_color < 0 ||
        state.messages[0].sender_color >= TG_GUI_AVATAR_COLORS) {
        puts("gui driver self-test: avatar colour out of range");
        return 2;
    }
    if (tg_gui_driver_color_for("Alice") != tg_gui_driver_color_for("Alice")) {
        puts("gui driver self-test: colour mapping not stable");
        return 2;
    }

    /* Ring overflow: push well past capacity, count caps and the oldest drops. */
    for (i = 0; i < TG_GUI_MAX_MESSAGES + 10; ++i) {
        tg_gui_driver_emit(&driver, 1700000000UL, 1, 0, 0, "Mario", "Io", 0,
                           "flood");
    }
    if (state.message_count != TG_GUI_MAX_MESSAGES) {
        printf("gui driver self-test: ring not capped (%d)\n",
               state.message_count);
        return 2;
    }
    if (strcmp(state.messages[TG_GUI_MAX_MESSAGES - 1].text, "flood") != 0) {
        puts("gui driver self-test: newest not retained after overflow");
        return 2;
    }

    /* Chat-list projection: rows -> tg_gui_state.chats (the sidebar). */
    {
        tg_chat_list_row list[4];

        memset(list, 0, sizeof(list));
        list[0].index = 1UL;
        strcpy(list[0].name, "Sviluppo AmigaIta");
        list[0].is_user = 0;
        list[0].is_current = 1;
        list[0].peer_id_lo = 0x100UL;
        list[1].index = 2UL;
        strcpy(list[1].name, "Mario Rossi");
        list[1].is_user = 1;
        list[1].unread = 2UL;
        list[1].peer_id_lo = 0x200UL;
        list[2].index = 3UL;
        strcpy(list[2].name, "Anna");
        list[2].is_user = 1;
        list[2].peer_id_lo = 0x300UL;
        list[3].index = 4UL;
        strcpy(list[3].name, "carla");
        list[3].name_is_username = 1;
        list[3].is_user = 1;
        list[3].peer_id_lo = 0x400UL;

        driver.on_chat_list_changed(driver.ctx, list, 4);

        if (state.chat_count != 4) {
            printf("gui driver self-test: chat_count %d != 4\n",
                   state.chat_count);
            return 2;
        }
        if (strcmp(state.chats[0].name, "Sviluppo AmigaIta") != 0 ||
            strcmp(state.chats[0].initials, "SA") != 0 ||
            state.selected_chat != 0) {
            puts("gui driver self-test: chat[0] (group/current) wrong");
            return 2;
        }
        if (state.chats[1].unread != 2 ||
            strcmp(state.chats[1].initials, "MR") != 0) {
            puts("gui driver self-test: chat[1] (unread/initials) wrong");
            return 2;
        }
        if (strcmp(state.chats[2].initials, "A") != 0) {
            puts("gui driver self-test: chat[2] single-word initials wrong");
            return 2;
        }
        if (strcmp(state.chats[3].name, "@carla") != 0 ||
            strcmp(state.chats[3].initials, "C") != 0) {
            puts("gui driver self-test: chat[3] @username projection wrong");
            return 2;
        }
        if (state.chats[0].avatar_color < 0 ||
            state.chats[0].avatar_color >= TG_GUI_AVATAR_COLORS) {
            puts("gui driver self-test: chat avatar colour out of range");
            return 2;
        }

        /* Notifications bump the matching row's badge and flash it (unless it
           is the open chat); unknown peers are ignored. */
        {
            tg_chat_notify_entry note;

            memset(&note, 0, sizeof(note));
            note.peer_id_lo = 0x200UL; /* Mario Rossi, not selected */
            driver.on_notification(driver.ctx, &note);
            if (state.chats[1].unread != 3 || state.chats[1].flash != 1) {
                puts("gui driver self-test: notification did not flash row");
                return 2;
            }
            note.peer_id_lo = 0x100UL; /* the selected/open chat: bump, no flash */
            driver.on_notification(driver.ctx, &note);
            if (state.chats[0].unread != 1 || state.chats[0].flash != 0) {
                puts("gui driver self-test: open chat should not flash");
                return 2;
            }
            note.peer_id_lo = 0x999UL; /* not in the sidebar: ignored */
            driver.on_notification(driver.ctx, &note);
            if (state.chats[2].unread != 0 || state.chats[2].flash != 0 ||
                state.chats[3].unread != 0 || state.chats[3].flash != 0) {
                puts("gui driver self-test: unknown peer must not touch rows");
                return 2;
            }
        }
    }

    puts("gui driver self-test: ok (messages + chat list + notifications)");
    return 0;
}
