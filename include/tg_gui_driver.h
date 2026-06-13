/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * The GUI chat driver: the bridge that lets the chat engine feed the native
 * GUI. It implements tg_chat_driver.on_message (the seam from step 3) by
 * projecting each resolved tg_chat_message_row into a tg_gui_message appended
 * to a tg_gui_state -- instead of the console driver, which prints it. This is
 * what turns the demo GUI window into one that shows real conversation rows.
 *
 * Pure projection: no I/O, no network, host-testable. See tg_chat_engine.h
 * (tg_chat_driver / tg_chat_message_row) and tg_gui.h (tg_gui_state).
 */

#ifndef TG_GUI_DRIVER_H
#define TG_GUI_DRIVER_H

#include "tg_chat_engine.h"
#include "tg_gui.h"

typedef struct tg_gui_chat_driver {
    tg_gui_state *state; /* the GUI model this driver appends messages to */
} tg_gui_chat_driver;

/* Binds chat_driver (the engine-facing vtable) so its on_message appends to
   `state` through `gui`. After this, hand chat_driver to the engine; every row
   it emits lands in state->messages (newest last, oldest dropped past the
   ring's capacity). */
void tg_gui_chat_driver_bind(tg_gui_chat_driver *gui, tg_gui_state *state,
                             tg_chat_driver *chat_driver);

/* Maps a sender name to a stable avatar tint index [0, TG_GUI_AVATAR_COLORS).
   Exposed so the chat-list projection can colour the same name identically. */
int tg_gui_driver_color_for(const char *name);

/* Host-CI self-test: feeds synthetic rows through the driver and asserts the
   resulting tg_gui_state (sender, text, time, is_own, colour, ring overflow).
   Returns 0 on success. */
int tg_gui_driver_self_test(void);

#endif
