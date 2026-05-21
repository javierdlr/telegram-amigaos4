/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#if !defined(TG_AMIGAOS3_ENABLE_AMISSL)
#define TG_AMIGAOS3_ENABLE_AMISSL 0
#endif

#if !defined(TG_ENABLE_TLS)
#define TG_ENABLE_TLS 0
#endif

#if TG_AMIGAOS3_ENABLE_AMISSL || TG_ENABLE_TLS
#define TG_MTPROTO_BIGINT_USE_OPENSSL 1
#include <openssl/bn.h>
#else
#define TG_MTPROTO_BIGINT_USE_OPENSSL 0
#endif

#include "tg_mtproto_bigint.h"

#if TG_MTPROTO_BIGINT_USE_OPENSSL
static int tg_bigint_mod_exp_openssl(
    const unsigned char base[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char *exponent,
    unsigned long exponent_length,
    const unsigned char modulus[TG_MTPROTO_BIGINT_SIZE],
    unsigned char output[TG_MTPROTO_BIGINT_SIZE])
{
    BN_CTX *ctx;
    BIGNUM *bn_base;
    BIGNUM *bn_exponent;
    BIGNUM *bn_modulus;
    BIGNUM *bn_result;
    int result;
    int bytes;

    if (base == 0 || exponent == 0 || modulus == 0 || output == 0) {
        return 0;
    }
    if (exponent_length > TG_MTPROTO_BIGINT_EXP_MAX) {
        exponent_length = TG_MTPROTO_BIGINT_EXP_MAX;
    }

    result = 0;
    ctx = BN_CTX_new();
    bn_base = 0;
    bn_exponent = 0;
    bn_modulus = 0;
    bn_result = 0;
    if (ctx == 0) {
        return 0;
    }

    bn_base = BN_bin2bn(base, TG_MTPROTO_BIGINT_SIZE, 0);
    bn_exponent = BN_bin2bn(exponent, (int)exponent_length, 0);
    bn_modulus = BN_bin2bn(modulus, TG_MTPROTO_BIGINT_SIZE, 0);
    bn_result = BN_new();
    if (bn_base == 0 || bn_exponent == 0 || bn_modulus == 0 ||
        bn_result == 0) {
        goto done;
    }
    if (BN_mod_exp(bn_result, bn_base, bn_exponent, bn_modulus, ctx) != 1) {
        goto done;
    }

    bytes = BN_num_bytes(bn_result);
    if (bytes < 0 || bytes > (int)TG_MTPROTO_BIGINT_SIZE) {
        goto done;
    }
    memset(output, 0, TG_MTPROTO_BIGINT_SIZE);
    if (bytes > 0) {
        BN_bn2bin(bn_result, output + TG_MTPROTO_BIGINT_SIZE -
                  (unsigned long)bytes);
    }
    result = 1;

done:
    if (bn_result != 0) {
        BN_clear_free(bn_result);
    }
    if (bn_modulus != 0) {
        BN_clear_free(bn_modulus);
    }
    if (bn_exponent != 0) {
        BN_clear_free(bn_exponent);
    }
    if (bn_base != 0) {
        BN_clear_free(bn_base);
    }
    BN_CTX_free(ctx);
    return result;
}
#endif

static int tg_bigint_bit(const unsigned char *a,
                         unsigned long a_length,
                         unsigned long bit)
{
    unsigned long byte_index;
    unsigned int bit_index;

    if (a == 0 || a_length == 0UL) {
        return 0;
    }
    byte_index = a_length - 1UL - (bit / 8UL);
    bit_index = (unsigned int)(bit % 8UL);
    return (a[byte_index] & (1U << bit_index)) != 0U;
}

static int tg_bigint_cmp_len(const unsigned char *a,
                             const unsigned char *b,
                             unsigned long length)
{
    unsigned long i;

    for (i = 0UL; i < length; ++i) {
        if (a[i] < b[i]) {
            return -1;
        }
        if (a[i] > b[i]) {
            return 1;
        }
    }
    return 0;
}

static void tg_bigint_sub_len(unsigned char *a,
                              const unsigned char *b,
                              unsigned long length)
{
    long i;
    unsigned int borrow;

    borrow = 0U;
    for (i = (long)length - 1L; i >= 0L; --i) {
        unsigned int av;
        unsigned int bv;
        av = (unsigned int)a[i];
        bv = (unsigned int)b[i] + borrow;
        if (av < bv) {
            a[i] = (unsigned char)(256U + av - bv);
            borrow = 1U;
        } else {
            a[i] = (unsigned char)(av - bv);
            borrow = 0U;
        }
    }
}

static void tg_bigint_mod_reduce(
    const unsigned char input[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char modulus[TG_MTPROTO_BIGINT_SIZE],
    unsigned char out[TG_MTPROTO_BIGINT_SIZE])
{
    unsigned char one[TG_MTPROTO_BIGINT_SIZE];
    unsigned long bit;

    memset(out, 0, TG_MTPROTO_BIGINT_SIZE);
    memset(one, 0, sizeof(one));
    one[TG_MTPROTO_BIGINT_SIZE - 1U] = 1U;
    for (bit = TG_MTPROTO_BIGINT_SIZE * 8UL; bit > 0UL; --bit) {
        tg_mtproto_bigint_add_mod(out, out, modulus);
        if (tg_bigint_bit(input, TG_MTPROTO_BIGINT_SIZE, bit - 1UL)) {
            tg_mtproto_bigint_add_mod(out, one, modulus);
        }
    }
}

int tg_mtproto_bigint_cmp(
    const unsigned char a[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char b[TG_MTPROTO_BIGINT_SIZE])
{
    return tg_bigint_cmp_len(a, b, TG_MTPROTO_BIGINT_SIZE);
}

void tg_mtproto_bigint_sub(
    unsigned char a[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char b[TG_MTPROTO_BIGINT_SIZE])
{
    tg_bigint_sub_len(a, b, TG_MTPROTO_BIGINT_SIZE);
}

void tg_mtproto_bigint_add_mod(
    unsigned char a[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char b[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char modulus[TG_MTPROTO_BIGINT_SIZE])
{
    unsigned char sum[TG_MTPROTO_BIGINT_SIZE + 1U];
    unsigned char mod[TG_MTPROTO_BIGINT_SIZE + 1U];
    long i;
    unsigned int carry;

    memset(sum, 0, sizeof(sum));
    memset(mod, 0, sizeof(mod));
    memcpy(mod + 1U, modulus, TG_MTPROTO_BIGINT_SIZE);
    carry = 0U;
    for (i = (long)TG_MTPROTO_BIGINT_SIZE - 1L; i >= 0L; --i) {
        unsigned int value;
        value = (unsigned int)a[i] + (unsigned int)b[i] + carry;
        sum[(unsigned long)i + 1UL] = (unsigned char)(value & 0xffU);
        carry = value >> 8;
    }
    sum[0] = (unsigned char)carry;
    if (tg_bigint_cmp_len(sum, mod, sizeof(sum)) >= 0) {
        tg_bigint_sub_len(sum, mod, sizeof(sum));
    }
    memcpy(a, sum + 1U, TG_MTPROTO_BIGINT_SIZE);
}

void tg_mtproto_bigint_sub_mod(
    unsigned char out[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char a[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char b[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char modulus[TG_MTPROTO_BIGINT_SIZE])
{
    memcpy(out, a, TG_MTPROTO_BIGINT_SIZE);
    if (tg_mtproto_bigint_cmp(out, b) >= 0) {
        tg_mtproto_bigint_sub(out, b);
    } else {
        unsigned char tmp[TG_MTPROTO_BIGINT_SIZE];
        memcpy(tmp, modulus, sizeof(tmp));
        tg_mtproto_bigint_sub(tmp, b);
        tg_mtproto_bigint_add_mod(out, tmp, modulus);
    }
}

void tg_mtproto_bigint_mod_mul(
    const unsigned char a[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char b[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char modulus[TG_MTPROTO_BIGINT_SIZE],
    unsigned char out[TG_MTPROTO_BIGINT_SIZE])
{
    unsigned char result[TG_MTPROTO_BIGINT_SIZE];
    unsigned char addend[TG_MTPROTO_BIGINT_SIZE];
    unsigned long bit;

    memset(result, 0, sizeof(result));
    tg_bigint_mod_reduce(a, modulus, addend);
    for (bit = 0UL; bit < TG_MTPROTO_BIGINT_SIZE * 8UL; ++bit) {
        if (tg_bigint_bit(b, TG_MTPROTO_BIGINT_SIZE, bit)) {
            tg_mtproto_bigint_add_mod(result, addend, modulus);
        }
        tg_mtproto_bigint_add_mod(addend, addend, modulus);
    }
    memcpy(out, result, TG_MTPROTO_BIGINT_SIZE);
}

void tg_mtproto_bigint_mod_exp(
    const unsigned char base[TG_MTPROTO_BIGINT_SIZE],
    const unsigned char *exponent,
    unsigned long exponent_length,
    const unsigned char modulus[TG_MTPROTO_BIGINT_SIZE],
    unsigned char output[TG_MTPROTO_BIGINT_SIZE])
{
    unsigned char result[TG_MTPROTO_BIGINT_SIZE];
    unsigned char power[TG_MTPROTO_BIGINT_SIZE];
    unsigned long bit;

#if TG_MTPROTO_BIGINT_USE_OPENSSL
    if (tg_bigint_mod_exp_openssl(base, exponent, exponent_length, modulus,
                                  output)) {
        return;
    }
#endif

    memset(result, 0, sizeof(result));
    result[TG_MTPROTO_BIGINT_SIZE - 1U] = 1U;
    tg_bigint_mod_reduce(base, modulus, power);
    if (exponent_length > TG_MTPROTO_BIGINT_EXP_MAX) {
        exponent_length = TG_MTPROTO_BIGINT_EXP_MAX;
    }
    for (bit = 0UL; bit < exponent_length * 8UL; ++bit) {
        if (tg_bigint_bit(exponent, exponent_length, bit)) {
            tg_mtproto_bigint_mod_mul(result, power, modulus, result);
        }
        tg_mtproto_bigint_mod_mul(power, power, modulus, power);
    }
    memcpy(output, result, TG_MTPROTO_BIGINT_SIZE);
}

void tg_mtproto_bigint_from_u32(
    unsigned long value,
    unsigned char out[TG_MTPROTO_BIGINT_SIZE])
{
    memset(out, 0, TG_MTPROTO_BIGINT_SIZE);
    out[TG_MTPROTO_BIGINT_SIZE - 4U] = (unsigned char)((value >> 24) & 0xffU);
    out[TG_MTPROTO_BIGINT_SIZE - 3U] = (unsigned char)((value >> 16) & 0xffU);
    out[TG_MTPROTO_BIGINT_SIZE - 2U] = (unsigned char)((value >> 8) & 0xffU);
    out[TG_MTPROTO_BIGINT_SIZE - 1U] = (unsigned char)(value & 0xffU);
}

void tg_mtproto_bigint_mul_bytes(
    const unsigned char *a,
    unsigned long a_length,
    const unsigned char *b,
    unsigned long b_length,
    unsigned char *out,
    unsigned long out_length)
{
    unsigned long ia;

    if (out == 0 || out_length == 0UL) {
        return;
    }
    memset(out, 0, (size_t)out_length);
    if (a == 0 || b == 0 || a_length == 0UL || b_length == 0UL) {
        return;
    }
    for (ia = 0UL; ia < a_length && ia < out_length; ++ia) {
        unsigned long ib;
        unsigned long carry;
        unsigned int av;
        av = (unsigned int)a[a_length - 1UL - ia];
        carry = 0UL;
        for (ib = 0UL; ib < b_length && ia + ib < out_length; ++ib) {
            unsigned long out_pos;
            unsigned long value;
            out_pos = out_length - 1UL - (ia + ib);
            value = (unsigned long)out[out_pos] +
                    ((unsigned long)av *
                     (unsigned long)b[b_length - 1UL - ib]) +
                    carry;
            out[out_pos] = (unsigned char)(value & 0xffUL);
            carry = value >> 8;
        }
        for (ib = ia + b_length; carry != 0UL && ib < out_length; ++ib) {
            unsigned long out_pos;
            unsigned long value;
            out_pos = out_length - 1UL - ib;
            value = (unsigned long)out[out_pos] + carry;
            out[out_pos] = (unsigned char)(value & 0xffUL);
            carry = value >> 8;
        }
    }
}

void tg_mtproto_bigint_add_bytes(
    unsigned char *a,
    unsigned long a_length,
    const unsigned char *b,
    unsigned long b_length)
{
    unsigned long i;
    unsigned int carry;

    if (a == 0 || b == 0 || a_length == 0UL || b_length == 0UL) {
        return;
    }
    carry = 0U;
    for (i = 0UL; i < a_length; ++i) {
        unsigned long ai;
        unsigned int bv;
        unsigned int value;
        ai = a_length - 1UL - i;
        bv = 0U;
        if (i < b_length) {
            bv = (unsigned int)b[b_length - 1UL - i];
        }
        value = (unsigned int)a[ai] + bv + carry;
        a[ai] = (unsigned char)(value & 0xffU);
        carry = value >> 8;
    }
}

unsigned long tg_mtproto_bigint_trim(
    const unsigned char *data,
    unsigned long data_length)
{
    unsigned long offset;

    offset = 0UL;
    while (offset + 1UL < data_length && data[offset] == 0U) {
        ++offset;
    }
    return offset;
}

int tg_mtproto_bigint_self_test(void)
{
    unsigned char a[TG_MTPROTO_BIGINT_SIZE];
    unsigned char b[TG_MTPROTO_BIGINT_SIZE];
    unsigned char m[TG_MTPROTO_BIGINT_SIZE];
    unsigned char out[TG_MTPROTO_BIGINT_SIZE];
    unsigned char exp[1];
    unsigned char mul_out[8];
    unsigned char mul_a[2];
    unsigned char mul_b[1];

    tg_mtproto_bigint_from_u32(5UL, a);
    tg_mtproto_bigint_from_u32(9UL, b);
    tg_mtproto_bigint_from_u32(13UL, m);
    tg_mtproto_bigint_add_mod(a, b, m);
    if (a[TG_MTPROTO_BIGINT_SIZE - 1U] != 1U) {
        return 1;
    }

    tg_mtproto_bigint_from_u32(3UL, a);
    tg_mtproto_bigint_from_u32(4UL, b);
    tg_mtproto_bigint_mod_mul(a, b, m, out);
    if (out[TG_MTPROTO_BIGINT_SIZE - 1U] != 12U) {
        return 1;
    }

    tg_mtproto_bigint_from_u32(2UL, a);
    tg_mtproto_bigint_from_u32(17UL, m);
    exp[0] = 10U;
    tg_mtproto_bigint_mod_exp(a, exp, sizeof(exp), m, out);
    if (out[TG_MTPROTO_BIGINT_SIZE - 1U] != 4U) {
        return 1;
    }

    tg_mtproto_bigint_from_u32(3UL, a);
    tg_mtproto_bigint_from_u32(5UL, b);
    tg_mtproto_bigint_from_u32(13UL, m);
    tg_mtproto_bigint_sub_mod(out, a, b, m);
    if (out[TG_MTPROTO_BIGINT_SIZE - 1U] != 11U) {
        return 1;
    }

    mul_a[0] = 0x12U;
    mul_a[1] = 0x34U;
    mul_b[0] = 0x10U;
    tg_mtproto_bigint_mul_bytes(mul_a, sizeof(mul_a), mul_b, sizeof(mul_b),
                                mul_out, sizeof(mul_out));
    if (mul_out[5] != 0x01U || mul_out[6] != 0x23U ||
        mul_out[7] != 0x40U) {
        return 1;
    }

    return 0;
}
