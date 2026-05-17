/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "tg_mtproto.h"
#include "tg_mtproto_crypto.h"
#include "tg_mtproto_envelope.h"
#include "tg_mtproto_session.h"
#include "tg_mtproto_tl.h"
#include "tg_mtproto_transport.h"

int tg_mtproto_self_test(void)
{
    if (tg_mtproto_tl_self_test() != 0) {
        puts("mtproto tl self-test: failed");
        return 2;
    }
    puts("mtproto tl self-test: ok");

    if (tg_mtproto_envelope_self_test() != 0) {
        puts("mtproto envelope self-test: failed");
        return 2;
    }
    puts("mtproto envelope self-test: ok");

    if (tg_mtproto_transport_self_test() != 0) {
        puts("mtproto transport self-test: failed");
        return 2;
    }
    puts("mtproto transport self-test: ok");

    if (tg_mtproto_crypto_self_test() != 0) {
        puts("mtproto crypto self-test: failed");
        return 2;
    }
    puts("mtproto crypto self-test: ok");

    if (tg_mtproto_session_self_test() != 0) {
        puts("mtproto session self-test: failed");
        return 2;
    }
    puts("mtproto session self-test: ok");

    puts("mtproto self-test: ok");
    return 0;
}
