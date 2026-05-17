/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "tg_mtproto.h"
#include "tg_mtproto_auth.h"
#include "tg_mtproto_crypto.h"
#include "tg_mtproto_dc.h"
#include "tg_mtproto_encrypted.h"
#include "tg_mtproto_envelope.h"
#include "tg_mtproto_login.h"
#include "tg_mtproto_message_id.h"
#include "tg_mtproto_probe.h"
#include "tg_mtproto_rsa.h"
#include "tg_mtproto_session.h"
#include "tg_mtproto_tl.h"
#include "tg_mtproto_transport.h"

int tg_mtproto_self_test(void)
{
    if (tg_mtproto_dc_self_test() != 0) {
        puts("mtproto dc self-test: failed");
        return 2;
    }
    puts("mtproto dc self-test: ok");

    if (tg_mtproto_message_id_self_test() != 0) {
        puts("mtproto message-id self-test: failed");
        return 2;
    }
    puts("mtproto message-id self-test: ok");

    if (tg_mtproto_auth_self_test() != 0) {
        puts("mtproto auth self-test: failed");
        return 2;
    }
    puts("mtproto auth self-test: ok");

    if (tg_mtproto_rsa_self_test() != 0) {
        puts("mtproto rsa self-test: failed");
        return 2;
    }
    puts("mtproto rsa self-test: ok");

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

    if (tg_mtproto_encrypted_self_test() != 0) {
        puts("mtproto encrypted self-test: failed");
        return 2;
    }
    puts("mtproto encrypted self-test: ok");

    if (tg_mtproto_transport_self_test() != 0) {
        puts("mtproto transport self-test: failed");
        return 2;
    }
    puts("mtproto transport self-test: ok");

    if (tg_mtproto_login_self_test() != 0) {
        puts("mtproto login self-test: failed");
        return 2;
    }
    puts("mtproto login self-test: ok");

    if (tg_mtproto_probe_self_test() != 0) {
        puts("mtproto probe self-test: failed");
        return 2;
    }
    puts("mtproto probe self-test: ok");

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
