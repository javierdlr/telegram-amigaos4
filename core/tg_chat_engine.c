/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Driver-agnostic chat-session model. See tg_chat_engine.h. This first slice
 * holds the updates cursor and the /diff flag; behaviour is unchanged from the
 * probe.c file-statics it replaces.
 */

#include "tg_chat_engine.h"

#include <stdio.h>
#include <string.h>

void tg_chat_engine_init(tg_chat_engine *engine)
{
    if (engine == 0) {
        return;
    }
    memset(&engine->updates_state, 0, sizeof(engine->updates_state));
    engine->diff_enabled = 1;
}

int tg_chat_engine_self_test(void)
{
    tg_chat_engine engine;

    /* Poison, then init must produce a clean session. */
    engine.diff_enabled = 0;
    engine.updates_state.pts = 12345UL;
    engine.updates_state.qts = 6789UL;
    engine.updates_state.date = 1111UL;
    engine.updates_state.seq = 2222UL;

    tg_chat_engine_init(&engine);

    if (engine.diff_enabled != 1) {
        puts("chat engine self-test: diff flag default wrong");
        return 2;
    }
    if (engine.updates_state.pts != 0UL || engine.updates_state.qts != 0UL ||
        engine.updates_state.date != 0UL || engine.updates_state.seq != 0UL) {
        puts("chat engine self-test: cursor not zeroed");
        return 2;
    }

    puts("chat engine self-test: ok (cursor zeroed, diff on)");
    return 0;
}
