/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Driver-agnostic chat-session model (Phase 5b), being extracted incrementally
 * from core/tg_mtproto_probe.c with the console/TUI as the behavioural oracle.
 * The goal: one chat "engine" that both the console driver and the GUI driver
 * sit on top of, instead of the chat logic printing straight to a FILE*.
 *
 * This is the first slice. It owns only the updates cursor (pts/qts/date/seq)
 * and the /diff flag -- the two file-statics that previously lived in probe.c.
 * Later slices fold in the notify queue, the chat-list model, selection/cursor,
 * the tick()/submit_line() event API and the driver callback vtable. See
 * docs/GUI_ARCHITECTURE.md.
 */

#ifndef TG_CHAT_ENGINE_H
#define TG_CHAT_ENGINE_H

#include "tg_mtproto_login.h" /* tg_mtproto_updates_state */

typedef struct tg_chat_engine {
    tg_mtproto_updates_state updates_state; /* getDifference cursor */
    int diff_enabled;                       /* background catch-up (/diff) */
} tg_chat_engine;

/* Resets the engine for a fresh chat session: zero cursor, catch-up enabled.
   Mirrors the previous per-session reset of the probe.c file-statics. */
void tg_chat_engine_init(tg_chat_engine *engine);

/* Portable invariant self-test (host-CI runnable). Returns 0 on success. */
int tg_chat_engine_self_test(void);

#endif
