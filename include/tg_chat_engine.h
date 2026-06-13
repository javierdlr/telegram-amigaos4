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

/* Cross-chat notification model. Entries are collected from live pushes and the
   getDifference drain, deduped by a ring of recent message ids, and consumed by
   the renderer (console today, GUI later). */
#define TG_CHAT_NOTIFY_MAX 8U
#define TG_CHAT_NOTIFY_TEXT 96U
/* Dedupe ring: must absorb the overlap between live pushes and the
   reconciliation sweep on a busy hour, not just push repeats. */
#define TG_CHAT_NOTIFY_RECENT 64U

typedef struct tg_chat_notify_entry {
    int is_chat; /* 1 = basic group (peer ids are the chat), 0 = DM user */
    unsigned long peer_id_hi;
    unsigned long peer_id_lo;
    /* The sender, for groups whose chat is not in the peer cache yet: a
       stale cache then still shows WHO wrote instead of a generic line. */
    unsigned long from_id_hi;
    unsigned long from_id_lo;
    char text[TG_CHAT_NOTIFY_TEXT];
} tg_chat_notify_entry;

typedef struct tg_chat_notify {
    tg_chat_notify_entry queue[TG_CHAT_NOTIFY_MAX];
    unsigned long count;
    unsigned long dropped;
    unsigned long recent_ids[TG_CHAT_NOTIFY_RECENT];
    unsigned long recent_pos;
    int armed; /* collection only happens while a chat session is open */
} tg_chat_notify;

typedef struct tg_chat_engine {
    tg_mtproto_updates_state updates_state; /* getDifference cursor */
    int diff_enabled;                       /* background catch-up (/diff) */
    tg_chat_notify notify;                  /* cross-chat notification queue */
} tg_chat_engine;

/* Notification queue ops (all NULL-safe so callers need no guards). */
/* Clears the queue + dedupe ring and sets the armed flag. */
void tg_chat_notify_reset(tg_chat_notify *notify, int armed);
/* Returns 1 if message_id was already seen (records it otherwise). id 0 is
   never deduped. A NULL queue returns 1 (treat as seen -> caller skips). */
int tg_chat_notify_seen(tg_chat_notify *notify, unsigned long message_id);
/* Claims the next free entry, incrementing count; returns NULL (and bumps
   dropped) when the queue is full or the pointer is NULL. The caller fills the
   returned entry's fields. */
tg_chat_notify_entry *tg_chat_notify_claim(tg_chat_notify *notify);

/* Resets the engine for a fresh chat session: zero cursor, catch-up enabled.
   Mirrors the previous per-session reset of the probe.c file-statics. */
void tg_chat_engine_init(tg_chat_engine *engine);

/* Portable invariant self-test (host-CI runnable). Returns 0 on success. */
int tg_chat_engine_self_test(void);

#endif
