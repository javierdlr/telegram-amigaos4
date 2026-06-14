/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Live GUI session bridge (Phase 5b, slice 3). A thin, non-interactive driver
 * over the same MTProto chat core the console uses: it holds an authenticated
 * connection for the lifetime of the GUI window and exposes a non-blocking
 * "tick" the window event loop pumps to harvest cross-chat notifications into
 * the sidebar badges. The implementation lives at the bottom of
 * core/tg_mtproto_probe.c so it reaches that file's static network helpers
 * directly -- no de-static of the network core. See docs/GUI_ARCHITECTURE.md.
 *
 * Single session per process (one window): the state is a file-static
 * singleton; these calls are not re-entrant.
 */

#ifndef TG_GUI_SESSION_H
#define TG_GUI_SESSION_H

#include <stdio.h>

#include "tg_gui.h" /* tg_gui_state */

/* Opens a live session over a saved authorization: derives the production
   endpoint from the auth file's DC, loads the api id, holds an authenticated
   connection, binds the GUI driver to `state`, and projects the current peer
   cache into the sidebar. The caller may refresh the cache from the network
   first (tg_mtproto_gui_refresh_peer_cache). Returns 0 on success, non-zero
   when the session could not be opened (the window can still run read-only).
   `state` must outlive the session. */
int tg_gui_session_open(const char *api_file, const char *auth_file,
                        const char *peer_cache_file, tg_gui_state *state,
                        FILE *stream);

/* One non-blocking poll cycle: a cadence-gated getDifference drain harvests
   inbound messages into the notify queue, which is then dispatched to the GUI
   driver (bumping + flashing the matching sidebar badge). Returns 1 when the
   GUI state changed (the caller should repaint), 0 otherwise. Safe to call
   when no session is open (returns 0). */
int tg_gui_session_tick(FILE *stream);

/* Opens the chat at the given 1-based peer-cache index: clears the transcript,
   fetches its recent history (incoming + outgoing) into tg_gui_state.messages
   through the GUI driver, and marks it the open chat so tg_gui_session_tick
   streams its new messages live. Returns 1 (always repaint) when a session is
   open, 0 otherwise. */
int tg_gui_session_open_chat(unsigned long peer_index, FILE *stream);

/* Closes the held connection and unbinds the notification queue. */
void tg_gui_session_close(void);

#endif
