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

void tg_gui_chat_driver_bind(tg_gui_chat_driver *gui, tg_gui_state *state,
                             tg_chat_driver *chat_driver)
{
    if (gui == 0 || chat_driver == 0) {
        return;
    }
    gui->state = state;
    chat_driver->ctx = gui;
    chat_driver->on_message = tg_gui_driver_on_message;
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

    puts("gui driver self-test: ok (rows projected to tg_gui_state)");
    return 0;
}
