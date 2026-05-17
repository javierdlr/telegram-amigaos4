/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_MTPROTO_RSA_H
#define TG_MTPROTO_RSA_H

#include "tg_mtproto_auth.h"
#include "tg_mtproto_tl.h"

#define TG_MTPROTO_RSA_MODULUS_LENGTH 256U
#define TG_MTPROTO_RSA_PADDED_LENGTH 256U

typedef struct tg_mtproto_public_key {
    tg_mtproto_fingerprint fingerprint;
    unsigned char modulus[TG_MTPROTO_RSA_MODULUS_LENGTH];
    unsigned long exponent;
} tg_mtproto_public_key;

const tg_mtproto_public_key *tg_mtproto_builtin_public_keys(
    unsigned int *count);

const tg_mtproto_public_key *tg_mtproto_select_public_key(
    const tg_mtproto_res_pq *res_pq);

tg_mtproto_tl_status tg_mtproto_build_p_q_inner_data_dc(
    tg_mtproto_tl_writer *writer,
    const unsigned char *pq,
    unsigned long pq_length,
    const unsigned char *p,
    unsigned long p_length,
    const unsigned char *q,
    unsigned long q_length,
    const unsigned char nonce[16],
    const unsigned char server_nonce[16],
    const unsigned char new_nonce[32],
    long dc_id);

tg_mtproto_tl_status tg_mtproto_rsa_pad(
    const unsigned char *data,
    unsigned long data_length,
    const unsigned char random_padding[96],
    const unsigned char temp_key[32],
    const tg_mtproto_public_key *public_key,
    unsigned char encrypted_data[TG_MTPROTO_RSA_PADDED_LENGTH]);

tg_mtproto_tl_status tg_mtproto_build_req_dh_params(
    tg_mtproto_tl_writer *writer,
    const unsigned char nonce[16],
    const unsigned char server_nonce[16],
    const unsigned char *p,
    unsigned long p_length,
    const unsigned char *q,
    unsigned long q_length,
    const tg_mtproto_fingerprint *fingerprint,
    const unsigned char encrypted_data[TG_MTPROTO_RSA_PADDED_LENGTH]);

int tg_mtproto_rsa_self_test(void);

#endif
