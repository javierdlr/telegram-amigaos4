/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_mtproto_crypto.h"

#define TG_SHA1_BLOCK_SIZE 64
#define TG_SHA256_BLOCK_SIZE 64

typedef struct tg_sha1_context {
    unsigned long h[5];
    unsigned char block[TG_SHA1_BLOCK_SIZE];
    unsigned long block_used;
    unsigned long length_hi;
    unsigned long length_lo;
} tg_sha1_context;

typedef struct tg_sha256_context {
    unsigned long h[8];
    unsigned char block[TG_SHA256_BLOCK_SIZE];
    unsigned long block_used;
    unsigned long length_hi;
    unsigned long length_lo;
} tg_sha256_context;

static unsigned long tg_rotl32(unsigned long value, unsigned long bits)
{
    value &= 0xffffffffUL;
    return ((value << bits) | (value >> (32UL - bits))) & 0xffffffffUL;
}

static unsigned long tg_rotr32(unsigned long value, unsigned long bits)
{
    value &= 0xffffffffUL;
    return ((value >> bits) | (value << (32UL - bits))) & 0xffffffffUL;
}

static unsigned long tg_load_be32(const unsigned char *data)
{
    return (((unsigned long)data[0]) << 24) |
           (((unsigned long)data[1]) << 16) |
           (((unsigned long)data[2]) << 8) |
           ((unsigned long)data[3]);
}

static void tg_store_be32(unsigned char *data, unsigned long value)
{
    data[0] = (unsigned char)((value >> 24) & 0xffUL);
    data[1] = (unsigned char)((value >> 16) & 0xffUL);
    data[2] = (unsigned char)((value >> 8) & 0xffUL);
    data[3] = (unsigned char)(value & 0xffUL);
}

static void tg_hash_add_length(tg_sha1_context *context, unsigned long data_length)
{
    unsigned long old_lo;
    unsigned long add_hi;
    unsigned long add_lo;

    add_hi = (data_length >> 29) & 0xffffffffUL;
    add_lo = (data_length << 3) & 0xffffffffUL;
    old_lo = context->length_lo;
    context->length_lo = (context->length_lo + add_lo) & 0xffffffffUL;
    context->length_hi = (context->length_hi + add_hi +
                          (context->length_lo < old_lo ? 1UL : 0UL)) &
                         0xffffffffUL;
}

static void tg_hash_add_length_256(tg_sha256_context *context,
                                   unsigned long data_length)
{
    unsigned long old_lo;
    unsigned long add_hi;
    unsigned long add_lo;

    add_hi = (data_length >> 29) & 0xffffffffUL;
    add_lo = (data_length << 3) & 0xffffffffUL;
    old_lo = context->length_lo;
    context->length_lo = (context->length_lo + add_lo) & 0xffffffffUL;
    context->length_hi = (context->length_hi + add_hi +
                          (context->length_lo < old_lo ? 1UL : 0UL)) &
                         0xffffffffUL;
}

static void tg_sha1_transform(tg_sha1_context *context,
                              const unsigned char block[TG_SHA1_BLOCK_SIZE])
{
    unsigned long w[80];
    unsigned long a;
    unsigned long b;
    unsigned long c;
    unsigned long d;
    unsigned long e;
    unsigned long f;
    unsigned long k;
    unsigned long temp;
    int i;

    for (i = 0; i < 16; ++i) {
        w[i] = tg_load_be32(block + (i * 4));
    }
    for (i = 16; i < 80; ++i) {
        w[i] = tg_rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    a = context->h[0];
    b = context->h[1];
    c = context->h[2];
    d = context->h[3];
    e = context->h[4];

    for (i = 0; i < 80; ++i) {
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5a827999UL;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1UL;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdcUL;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6UL;
        }
        temp = (tg_rotl32(a, 5) + f + e + k + w[i]) & 0xffffffffUL;
        e = d;
        d = c;
        c = tg_rotl32(b, 30);
        b = a;
        a = temp;
    }

    context->h[0] = (context->h[0] + a) & 0xffffffffUL;
    context->h[1] = (context->h[1] + b) & 0xffffffffUL;
    context->h[2] = (context->h[2] + c) & 0xffffffffUL;
    context->h[3] = (context->h[3] + d) & 0xffffffffUL;
    context->h[4] = (context->h[4] + e) & 0xffffffffUL;
}

static void tg_sha1_init(tg_sha1_context *context)
{
    context->h[0] = 0x67452301UL;
    context->h[1] = 0xefcdab89UL;
    context->h[2] = 0x98badcfeUL;
    context->h[3] = 0x10325476UL;
    context->h[4] = 0xc3d2e1f0UL;
    context->block_used = 0;
    context->length_hi = 0;
    context->length_lo = 0;
}

static void tg_sha1_update(tg_sha1_context *context, const unsigned char *data,
                           unsigned long data_length)
{
    unsigned long take;

    tg_hash_add_length(context, data_length);
    while (data_length > 0) {
        take = TG_SHA1_BLOCK_SIZE - context->block_used;
        if (take > data_length) {
            take = data_length;
        }
        memcpy(context->block + context->block_used, data, (size_t)take);
        context->block_used += take;
        data += take;
        data_length -= take;
        if (context->block_used == TG_SHA1_BLOCK_SIZE) {
            tg_sha1_transform(context, context->block);
            context->block_used = 0;
        }
    }
}

static void tg_sha1_final(tg_sha1_context *context,
                          unsigned char digest[TG_MTPROTO_SHA1_LENGTH])
{
    int i;

    context->block[context->block_used++] = 0x80U;
    if (context->block_used > 56UL) {
        while (context->block_used < TG_SHA1_BLOCK_SIZE) {
            context->block[context->block_used++] = 0;
        }
        tg_sha1_transform(context, context->block);
        context->block_used = 0;
    }
    while (context->block_used < 56UL) {
        context->block[context->block_used++] = 0;
    }
    tg_store_be32(context->block + 56, context->length_hi);
    tg_store_be32(context->block + 60, context->length_lo);
    tg_sha1_transform(context, context->block);

    for (i = 0; i < 5; ++i) {
        tg_store_be32(digest + (i * 4), context->h[i]);
    }
}

void tg_mtproto_sha1(const unsigned char *data, unsigned long data_length,
                     unsigned char digest[TG_MTPROTO_SHA1_LENGTH])
{
    tg_sha1_context context;

    tg_sha1_init(&context);
    if (data_length > 0) {
        tg_sha1_update(&context, data, data_length);
    }
    tg_sha1_final(&context, digest);
}

static void tg_sha256_transform(tg_sha256_context *context,
                                const unsigned char block[TG_SHA256_BLOCK_SIZE])
{
    static const unsigned long k[64] = {
        0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
        0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
        0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
        0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
        0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
        0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
        0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
        0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
        0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
        0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
        0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
        0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
        0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
        0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
        0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
        0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
    };
    unsigned long w[64];
    unsigned long a;
    unsigned long b;
    unsigned long c;
    unsigned long d;
    unsigned long e;
    unsigned long f;
    unsigned long g;
    unsigned long h;
    unsigned long s0;
    unsigned long s1;
    unsigned long ch;
    unsigned long maj;
    unsigned long temp1;
    unsigned long temp2;
    int i;

    for (i = 0; i < 16; ++i) {
        w[i] = tg_load_be32(block + (i * 4));
    }
    for (i = 16; i < 64; ++i) {
        s0 = tg_rotr32(w[i - 15], 7) ^ tg_rotr32(w[i - 15], 18) ^
             (w[i - 15] >> 3);
        s1 = tg_rotr32(w[i - 2], 17) ^ tg_rotr32(w[i - 2], 19) ^
             (w[i - 2] >> 10);
        w[i] = (w[i - 16] + s0 + w[i - 7] + s1) & 0xffffffffUL;
    }

    a = context->h[0];
    b = context->h[1];
    c = context->h[2];
    d = context->h[3];
    e = context->h[4];
    f = context->h[5];
    g = context->h[6];
    h = context->h[7];

    for (i = 0; i < 64; ++i) {
        s1 = tg_rotr32(e, 6) ^ tg_rotr32(e, 11) ^ tg_rotr32(e, 25);
        ch = (e & f) ^ ((~e) & g);
        temp1 = (h + s1 + ch + k[i] + w[i]) & 0xffffffffUL;
        s0 = tg_rotr32(a, 2) ^ tg_rotr32(a, 13) ^ tg_rotr32(a, 22);
        maj = (a & b) ^ (a & c) ^ (b & c);
        temp2 = (s0 + maj) & 0xffffffffUL;
        h = g;
        g = f;
        f = e;
        e = (d + temp1) & 0xffffffffUL;
        d = c;
        c = b;
        b = a;
        a = (temp1 + temp2) & 0xffffffffUL;
    }

    context->h[0] = (context->h[0] + a) & 0xffffffffUL;
    context->h[1] = (context->h[1] + b) & 0xffffffffUL;
    context->h[2] = (context->h[2] + c) & 0xffffffffUL;
    context->h[3] = (context->h[3] + d) & 0xffffffffUL;
    context->h[4] = (context->h[4] + e) & 0xffffffffUL;
    context->h[5] = (context->h[5] + f) & 0xffffffffUL;
    context->h[6] = (context->h[6] + g) & 0xffffffffUL;
    context->h[7] = (context->h[7] + h) & 0xffffffffUL;
}

static void tg_sha256_init(tg_sha256_context *context)
{
    context->h[0] = 0x6a09e667UL;
    context->h[1] = 0xbb67ae85UL;
    context->h[2] = 0x3c6ef372UL;
    context->h[3] = 0xa54ff53aUL;
    context->h[4] = 0x510e527fUL;
    context->h[5] = 0x9b05688cUL;
    context->h[6] = 0x1f83d9abUL;
    context->h[7] = 0x5be0cd19UL;
    context->block_used = 0;
    context->length_hi = 0;
    context->length_lo = 0;
}

static void tg_sha256_update(tg_sha256_context *context,
                             const unsigned char *data,
                             unsigned long data_length)
{
    unsigned long take;

    tg_hash_add_length_256(context, data_length);
    while (data_length > 0) {
        take = TG_SHA256_BLOCK_SIZE - context->block_used;
        if (take > data_length) {
            take = data_length;
        }
        memcpy(context->block + context->block_used, data, (size_t)take);
        context->block_used += take;
        data += take;
        data_length -= take;
        if (context->block_used == TG_SHA256_BLOCK_SIZE) {
            tg_sha256_transform(context, context->block);
            context->block_used = 0;
        }
    }
}

static void tg_sha256_final(tg_sha256_context *context,
                            unsigned char digest[TG_MTPROTO_SHA256_LENGTH])
{
    int i;

    context->block[context->block_used++] = 0x80U;
    if (context->block_used > 56UL) {
        while (context->block_used < TG_SHA256_BLOCK_SIZE) {
            context->block[context->block_used++] = 0;
        }
        tg_sha256_transform(context, context->block);
        context->block_used = 0;
    }
    while (context->block_used < 56UL) {
        context->block[context->block_used++] = 0;
    }
    tg_store_be32(context->block + 56, context->length_hi);
    tg_store_be32(context->block + 60, context->length_lo);
    tg_sha256_transform(context, context->block);

    for (i = 0; i < 8; ++i) {
        tg_store_be32(digest + (i * 4), context->h[i]);
    }
}

void tg_mtproto_sha256(const unsigned char *data, unsigned long data_length,
                       unsigned char digest[TG_MTPROTO_SHA256_LENGTH])
{
    tg_sha256_context context;

    tg_sha256_init(&context);
    if (data_length > 0) {
        tg_sha256_update(&context, data, data_length);
    }
    tg_sha256_final(&context, digest);
}

int tg_mtproto_crypto_self_test(void)
{
    static const unsigned char abc[] = { 'a', 'b', 'c' };
    static const unsigned char sha1_abc[TG_MTPROTO_SHA1_LENGTH] = {
        0xa9U, 0x99U, 0x3eU, 0x36U, 0x47U, 0x06U, 0x81U, 0x6aU,
        0xbaU, 0x3eU, 0x25U, 0x71U, 0x78U, 0x50U, 0xc2U, 0x6cU,
        0x9cU, 0xd0U, 0xd8U, 0x9dU
    };
    static const unsigned char sha256_abc[TG_MTPROTO_SHA256_LENGTH] = {
        0xbaU, 0x78U, 0x16U, 0xbfU, 0x8fU, 0x01U, 0xcfU, 0xeaU,
        0x41U, 0x41U, 0x40U, 0xdeU, 0x5dU, 0xaeU, 0x22U, 0x23U,
        0xb0U, 0x03U, 0x61U, 0xa3U, 0x96U, 0x17U, 0x7aU, 0x9cU,
        0xb4U, 0x10U, 0xffU, 0x61U, 0xf2U, 0x00U, 0x15U, 0xadU
    };
    unsigned char digest256[TG_MTPROTO_SHA256_LENGTH];
    unsigned char digest1[TG_MTPROTO_SHA1_LENGTH];

    tg_mtproto_sha1(abc, sizeof(abc), digest1);
    if (memcmp(digest1, sha1_abc, sizeof(sha1_abc)) != 0) {
        return 2;
    }
    tg_mtproto_sha256(abc, sizeof(abc), digest256);
    if (memcmp(digest256, sha256_abc, sizeof(sha256_abc)) != 0) {
        return 2;
    }
    return 0;
}
