/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_mtproto_bigint.h"
#include "tg_mtproto_srp.h"

#define TG_PASSWORD_KDF_ALGO_SRP_CONSTRUCTOR 0x3a912d4aUL
#define TG_SRP_PASSWORD_MAX 256U
#define TG_SRP_PBKDF2_ITERATIONS 100000UL

static int tg_srp_has_nonzero(const unsigned char *data,
                              unsigned long data_length)
{
    unsigned long i;

    if (data == 0 || data_length == 0UL) {
        return 0;
    }
    for (i = 0UL; i < data_length; ++i) {
        if (data[i] != 0U) {
            return 1;
        }
    }
    return 0;
}

static void tg_srp_pad_right(const unsigned char *data,
                             unsigned long data_length,
                             unsigned char out[TG_MTPROTO_SRP_VALUE_LENGTH])
{
    memset(out, 0, TG_MTPROTO_SRP_VALUE_LENGTH);
    if (data != 0 && data_length <= TG_MTPROTO_SRP_VALUE_LENGTH) {
        memcpy(out + TG_MTPROTO_SRP_VALUE_LENGTH - data_length, data,
               (size_t)data_length);
    }
}

static void tg_srp_sha256_salted(const unsigned char *data,
                                 unsigned long data_length,
                                 const unsigned char *salt,
                                 unsigned long salt_length,
                                 unsigned char out[TG_MTPROTO_SHA256_LENGTH])
{
    unsigned char buffer[(TG_MTPROTO_PASSWORD_BYTES_MAX * 2U) +
                         TG_SRP_PASSWORD_MAX];

    memcpy(buffer, salt, (size_t)salt_length);
    memcpy(buffer + salt_length, data, (size_t)data_length);
    memcpy(buffer + salt_length + data_length, salt, (size_t)salt_length);
    tg_mtproto_sha256(buffer, salt_length + data_length + salt_length, out);
}

static tg_mtproto_tl_status tg_srp_derive_x(
    const tg_mtproto_password_summary *password,
    const unsigned char *password_bytes,
    unsigned long password_length,
    unsigned long pbkdf2_iterations,
    unsigned char x[TG_MTPROTO_SHA256_LENGTH])
{
    unsigned char ph1a[TG_MTPROTO_SHA256_LENGTH];
    unsigned char ph1b[TG_MTPROTO_SHA256_LENGTH];
    unsigned char pbkdf2[TG_MTPROTO_SHA512_LENGTH];

    tg_srp_sha256_salted(password_bytes, password_length,
                         password->current_salt1,
                         password->current_salt1_length, ph1a);
    tg_srp_sha256_salted(ph1a, sizeof(ph1a),
                         password->current_salt2,
                         password->current_salt2_length, ph1b);
    if (tg_mtproto_pbkdf2_hmac_sha512(
            ph1b, sizeof(ph1b),
            password->current_salt1,
            password->current_salt1_length,
            pbkdf2_iterations,
            pbkdf2,
            sizeof(pbkdf2)) != 0) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    tg_srp_sha256_salted(pbkdf2, sizeof(pbkdf2),
                         password->current_salt2,
                         password->current_salt2_length, x);
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_srp_make_proof_iterations(
    const tg_mtproto_password_summary *password,
    const unsigned char *password_bytes,
    unsigned long password_length,
    const unsigned char random_a[TG_MTPROTO_SRP_VALUE_LENGTH],
    unsigned long pbkdf2_iterations,
    tg_mtproto_srp_proof *out)
{
    unsigned char p[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char g[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char one[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char b[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char k_hash[TG_MTPROTO_SHA256_LENGTH];
    unsigned char k[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char x[TG_MTPROTO_SHA256_LENGTH];
    unsigned char u[TG_MTPROTO_SHA256_LENGTH];
    unsigned char v[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char kv[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char base[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char exponent[TG_MTPROTO_SRP_EXP_LENGTH];
    unsigned char ux[TG_MTPROTO_SRP_EXP_LENGTH];
    unsigned char s[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char session_key[TG_MTPROTO_SHA256_LENGTH];
    unsigned char hp[TG_MTPROTO_SHA256_LENGTH];
    unsigned char hg[TG_MTPROTO_SHA256_LENGTH];
    unsigned char hs1[TG_MTPROTO_SHA256_LENGTH];
    unsigned char hs2[TG_MTPROTO_SHA256_LENGTH];
    unsigned char hash_input[(TG_MTPROTO_SHA256_LENGTH * 3U) +
                             (TG_MTPROTO_SRP_VALUE_LENGTH * 2U) +
                             TG_MTPROTO_SHA256_LENGTH];
    unsigned long offset;
    unsigned int i;

    if (password == 0 || password_bytes == 0 || random_a == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    if (password_length > TG_SRP_PASSWORD_MAX ||
        !password->has_current_algo ||
        password->current_algo_constructor !=
            TG_PASSWORD_KDF_ALGO_SRP_CONSTRUCTOR ||
        password->current_g == 0UL ||
        password->current_salt1_length > TG_MTPROTO_PASSWORD_BYTES_MAX ||
        password->current_salt2_length > TG_MTPROTO_PASSWORD_BYTES_MAX ||
        password->current_p_length == 0UL ||
        password->current_p_length > TG_MTPROTO_SRP_VALUE_LENGTH ||
        password->srp_b_length == 0UL ||
        password->srp_b_length > TG_MTPROTO_SRP_VALUE_LENGTH ||
        !tg_srp_has_nonzero(password->current_p, password->current_p_length) ||
        !tg_srp_has_nonzero(password->srp_b, password->srp_b_length) ||
        !tg_srp_has_nonzero(random_a, TG_MTPROTO_SRP_VALUE_LENGTH)) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }

    tg_srp_pad_right(password->current_p, password->current_p_length, p);
    tg_mtproto_bigint_from_u32(password->current_g, g);
    tg_srp_pad_right(password->srp_b, password->srp_b_length, b);
    tg_mtproto_bigint_from_u32(1UL, one);
    if (tg_mtproto_bigint_cmp(p, one) <= 0 ||
        tg_mtproto_bigint_cmp(g, p) >= 0 ||
        tg_mtproto_bigint_cmp(b, p) >= 0) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }

    if (tg_srp_derive_x(password, password_bytes, password_length,
                        pbkdf2_iterations, x) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }

    memcpy(hash_input, p, TG_MTPROTO_SRP_VALUE_LENGTH);
    memcpy(hash_input + TG_MTPROTO_SRP_VALUE_LENGTH, g,
           TG_MTPROTO_SRP_VALUE_LENGTH);
    tg_mtproto_sha256(hash_input, TG_MTPROTO_SRP_VALUE_LENGTH * 2UL, k_hash);
    tg_srp_pad_right(k_hash, sizeof(k_hash), k);

    tg_mtproto_bigint_mod_exp(g, random_a, TG_MTPROTO_SRP_VALUE_LENGTH,
                              p, out->a);
    out->a_length = TG_MTPROTO_SRP_VALUE_LENGTH;

    memcpy(hash_input, out->a, TG_MTPROTO_SRP_VALUE_LENGTH);
    memcpy(hash_input + TG_MTPROTO_SRP_VALUE_LENGTH, b,
           TG_MTPROTO_SRP_VALUE_LENGTH);
    tg_mtproto_sha256(hash_input, TG_MTPROTO_SRP_VALUE_LENGTH * 2UL, u);

    tg_mtproto_bigint_mod_exp(g, x, sizeof(x), p, v);
    tg_mtproto_bigint_mod_mul(k, v, p, kv);
    tg_mtproto_bigint_sub_mod(base, b, kv, p);

    tg_mtproto_bigint_mul_bytes(u, sizeof(u), x, sizeof(x),
                                ux, sizeof(ux));
    memcpy(exponent, ux, sizeof(exponent));
    tg_mtproto_bigint_add_bytes(exponent, sizeof(exponent),
                                random_a, TG_MTPROTO_SRP_VALUE_LENGTH);
    tg_mtproto_bigint_mod_exp(base, exponent, sizeof(exponent), p, s);
    tg_mtproto_sha256(s, sizeof(s), session_key);

    tg_mtproto_sha256(p, sizeof(p), hp);
    tg_mtproto_sha256(g, sizeof(g), hg);
    tg_mtproto_sha256(password->current_salt1, password->current_salt1_length,
                      hs1);
    tg_mtproto_sha256(password->current_salt2, password->current_salt2_length,
                      hs2);
    for (i = 0U; i < TG_MTPROTO_SHA256_LENGTH; ++i) {
        hash_input[i] = (unsigned char)(hp[i] ^ hg[i]);
    }
    offset = TG_MTPROTO_SHA256_LENGTH;
    memcpy(hash_input + offset, hs1, TG_MTPROTO_SHA256_LENGTH);
    offset += TG_MTPROTO_SHA256_LENGTH;
    memcpy(hash_input + offset, hs2, TG_MTPROTO_SHA256_LENGTH);
    offset += TG_MTPROTO_SHA256_LENGTH;
    memcpy(hash_input + offset, out->a, TG_MTPROTO_SRP_VALUE_LENGTH);
    offset += TG_MTPROTO_SRP_VALUE_LENGTH;
    memcpy(hash_input + offset, b, TG_MTPROTO_SRP_VALUE_LENGTH);
    offset += TG_MTPROTO_SRP_VALUE_LENGTH;
    memcpy(hash_input + offset, session_key, TG_MTPROTO_SHA256_LENGTH);
    offset += TG_MTPROTO_SHA256_LENGTH;
    tg_mtproto_sha256(hash_input, offset, out->m1);

    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_srp_make_proof(
    const tg_mtproto_password_summary *password,
    const unsigned char *password_bytes,
    unsigned long password_length,
    const unsigned char random_a[TG_MTPROTO_SRP_VALUE_LENGTH],
    tg_mtproto_srp_proof *out)
{
    return tg_srp_make_proof_iterations(password, password_bytes,
                                        password_length, random_a,
                                        TG_SRP_PBKDF2_ITERATIONS, out);
}

int tg_mtproto_srp_self_test(void)
{
    tg_mtproto_password_summary password;
    tg_mtproto_srp_proof proof;
    unsigned char random_a[TG_MTPROTO_SRP_VALUE_LENGTH];
    unsigned char password_bytes[4];
    unsigned int i;
    int m1_zero;

    memset(&password, 0, sizeof(password));
    password.has_current_algo = 1;
    password.current_algo_constructor = TG_PASSWORD_KDF_ALGO_SRP_CONSTRUCTOR;
    password.current_g = 3UL;
    password.current_salt1[0] = 0x11U;
    password.current_salt1[1] = 0x12U;
    password.current_salt1_length = 2UL;
    password.current_salt2[0] = 0x21U;
    password.current_salt2[1] = 0x22U;
    password.current_salt2[2] = 0x23U;
    password.current_salt2_length = 3UL;
    password.current_p[0] = 17U;
    password.current_p_length = 1UL;
    password.srp_b[0] = 15U;
    password.srp_b_length = 1UL;
    password.srp_id_lo = 1UL;
    memset(random_a, 0, sizeof(random_a));
    random_a[TG_MTPROTO_SRP_VALUE_LENGTH - 1U] = 7U;
    password_bytes[0] = 't';
    password_bytes[1] = 'e';
    password_bytes[2] = 's';
    password_bytes[3] = 't';

    if (tg_srp_make_proof_iterations(&password, password_bytes,
                                     sizeof(password_bytes), random_a,
                                     1UL, &proof) != TG_MTPROTO_TL_OK ||
        proof.a_length != TG_MTPROTO_SRP_VALUE_LENGTH ||
        !tg_srp_has_nonzero(proof.a, sizeof(proof.a))) {
        return 1;
    }
    m1_zero = 1;
    for (i = 0U; i < TG_MTPROTO_SHA256_LENGTH; ++i) {
        if (proof.m1[i] != 0U) {
            m1_zero = 0;
        }
    }
    if (m1_zero) {
        return 1;
    }

    password.current_algo_constructor = 0UL;
    if (tg_srp_make_proof_iterations(&password, password_bytes,
                                     sizeof(password_bytes), random_a,
                                     1UL, &proof) !=
        TG_MTPROTO_TL_INVALID_DATA) {
        return 1;
    }

    return 0;
}
