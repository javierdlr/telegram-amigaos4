/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_MTPROTO_SRP_H
#define TG_MTPROTO_SRP_H

#include "tg_mtproto_crypto.h"
#include "tg_mtproto_login.h"
#include "tg_mtproto_tl.h"

#define TG_MTPROTO_SRP_VALUE_LENGTH 256U
#define TG_MTPROTO_SRP_EXP_LENGTH 512U

typedef struct tg_mtproto_srp_proof {
    unsigned char a[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned long a_length;
    unsigned char m1[TG_MTPROTO_SHA256_LENGTH];
} tg_mtproto_srp_proof;

tg_mtproto_tl_status tg_mtproto_srp_make_proof(
    const tg_mtproto_password_summary *password,
    const unsigned char *password_bytes,
    unsigned long password_length,
    const unsigned char random_a[TG_MTPROTO_SRP_VALUE_LENGTH],
    tg_mtproto_srp_proof *out);

int tg_mtproto_srp_self_test(void);

#endif
