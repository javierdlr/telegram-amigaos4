/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_mtproto_crypto.h"
#include "tg_mtproto_rsa.h"

#define TG_AES_BLOCK_SIZE 16U
#define TG_RSA_DATA_PADDED_LENGTH 192U
#define TG_RSA_HASHED_LENGTH 224U
#define TG_SERVER_DH_PARAMS_OK_CONSTRUCTOR 0xd0e8075cUL
#define TG_SERVER_DH_INNER_DATA_CONSTRUCTOR 0xb5890dbaUL
#define TG_SET_CLIENT_DH_PARAMS_CONSTRUCTOR 0xf5045f1fUL
#define TG_CLIENT_DH_INNER_DATA_CONSTRUCTOR 0x6643b654UL
#define TG_DH_GEN_OK_CONSTRUCTOR 0x3bcbf734UL
#define TG_DH_GEN_RETRY_CONSTRUCTOR 0x46dc1fb9UL
#define TG_DH_GEN_FAIL_CONSTRUCTOR 0xa69dae02UL

static const unsigned char tg_aes_sbox[256] = {
    0x63U,0x7cU,0x77U,0x7bU,0xf2U,0x6bU,0x6fU,0xc5U,0x30U,0x01U,0x67U,0x2bU,0xfeU,0xd7U,0xabU,0x76U,
    0xcaU,0x82U,0xc9U,0x7dU,0xfaU,0x59U,0x47U,0xf0U,0xadU,0xd4U,0xa2U,0xafU,0x9cU,0xa4U,0x72U,0xc0U,
    0xb7U,0xfdU,0x93U,0x26U,0x36U,0x3fU,0xf7U,0xccU,0x34U,0xa5U,0xe5U,0xf1U,0x71U,0xd8U,0x31U,0x15U,
    0x04U,0xc7U,0x23U,0xc3U,0x18U,0x96U,0x05U,0x9aU,0x07U,0x12U,0x80U,0xe2U,0xebU,0x27U,0xb2U,0x75U,
    0x09U,0x83U,0x2cU,0x1aU,0x1bU,0x6eU,0x5aU,0xa0U,0x52U,0x3bU,0xd6U,0xb3U,0x29U,0xe3U,0x2fU,0x84U,
    0x53U,0xd1U,0x00U,0xedU,0x20U,0xfcU,0xb1U,0x5bU,0x6aU,0xcbU,0xbeU,0x39U,0x4aU,0x4cU,0x58U,0xcfU,
    0xd0U,0xefU,0xaaU,0xfbU,0x43U,0x4dU,0x33U,0x85U,0x45U,0xf9U,0x02U,0x7fU,0x50U,0x3cU,0x9fU,0xa8U,
    0x51U,0xa3U,0x40U,0x8fU,0x92U,0x9dU,0x38U,0xf5U,0xbcU,0xb6U,0xdaU,0x21U,0x10U,0xffU,0xf3U,0xd2U,
    0xcdU,0x0cU,0x13U,0xecU,0x5fU,0x97U,0x44U,0x17U,0xc4U,0xa7U,0x7eU,0x3dU,0x64U,0x5dU,0x19U,0x73U,
    0x60U,0x81U,0x4fU,0xdcU,0x22U,0x2aU,0x90U,0x88U,0x46U,0xeeU,0xb8U,0x14U,0xdeU,0x5eU,0x0bU,0xdbU,
    0xe0U,0x32U,0x3aU,0x0aU,0x49U,0x06U,0x24U,0x5cU,0xc2U,0xd3U,0xacU,0x62U,0x91U,0x95U,0xe4U,0x79U,
    0xe7U,0xc8U,0x37U,0x6dU,0x8dU,0xd5U,0x4eU,0xa9U,0x6cU,0x56U,0xf4U,0xeaU,0x65U,0x7aU,0xaeU,0x08U,
    0xbaU,0x78U,0x25U,0x2eU,0x1cU,0xa6U,0xb4U,0xc6U,0xe8U,0xddU,0x74U,0x1fU,0x4bU,0xbdU,0x8bU,0x8aU,
    0x70U,0x3eU,0xb5U,0x66U,0x48U,0x03U,0xf6U,0x0eU,0x61U,0x35U,0x57U,0xb9U,0x86U,0xc1U,0x1dU,0x9eU,
    0xe1U,0xf8U,0x98U,0x11U,0x69U,0xd9U,0x8eU,0x94U,0x9bU,0x1eU,0x87U,0xe9U,0xceU,0x55U,0x28U,0xdfU,
    0x8cU,0xa1U,0x89U,0x0dU,0xbfU,0xe6U,0x42U,0x68U,0x41U,0x99U,0x2dU,0x0fU,0xb0U,0x54U,0xbbU,0x16U
};

static const unsigned char tg_aes_rcon[15] = {
    0x00U,0x01U,0x02U,0x04U,0x08U,0x10U,0x20U,0x40U,0x80U,0x1bU,0x36U,0x6cU,0xd8U,0xabU,0x4dU
};

static const tg_mtproto_public_key tg_builtin_keys[] = {
    {
        {0xd09d1d85UL, 0xde64fd85UL},
        {
            0xe8U,0xbbU,0x33U,0x05U,0xc0U,0xb5U,0x2cU,0x6cU,0xf2U,0xafU,0xdfU,0x76U,0x37U,0x31U,0x34U,0x89U,
            0xe6U,0x3eU,0x05U,0x26U,0x8eU,0x5bU,0xadU,0xb6U,0x01U,0xafU,0x41U,0x77U,0x86U,0x47U,0x2eU,0x5fU,
            0x93U,0xb8U,0x54U,0x38U,0x96U,0x8eU,0x20U,0xe6U,0x72U,0x9aU,0x30U,0x1cU,0x0aU,0xfcU,0x12U,0x1bU,
            0xf7U,0x15U,0x1fU,0x83U,0x44U,0x36U,0xf7U,0xfdU,0xa6U,0x80U,0x84U,0x7aU,0x66U,0xbfU,0x64U,0xacU,
            0xceU,0xc7U,0x8eU,0xe2U,0x1cU,0x0bU,0x31U,0x6fU,0x0eU,0xdaU,0xfeU,0x2fU,0x41U,0x90U,0x8dU,0xa7U,
            0xbdU,0x1fU,0x4aU,0x51U,0x07U,0x63U,0x8eU,0xebU,0x67U,0x04U,0x0aU,0xceU,0x47U,0x2aU,0x14U,0xf9U,
            0x0dU,0x9fU,0x7cU,0x2bU,0x7dU,0xefU,0x99U,0x68U,0x8bU,0xa3U,0x07U,0x3aU,0xdbU,0x57U,0x50U,0xbbU,
            0x02U,0x96U,0x49U,0x02U,0xa3U,0x59U,0xfeU,0x74U,0x5dU,0x81U,0x70U,0xe3U,0x68U,0x76U,0xd4U,0xfdU,
            0x8aU,0x5dU,0x41U,0xb2U,0xa7U,0x6cU,0xbfU,0xf9U,0xa1U,0x32U,0x67U,0xebU,0x95U,0x80U,0xb2U,0xd0U,
            0x6dU,0x10U,0x35U,0x74U,0x48U,0xd2U,0x0dU,0x9dU,0xa2U,0x19U,0x1cU,0xb5U,0xd8U,0xc9U,0x39U,0x82U,
            0x96U,0x1cU,0xdfU,0xdeU,0xdaU,0x62U,0x9eU,0x37U,0xf1U,0xfbU,0x09U,0xa0U,0x72U,0x20U,0x27U,0x69U,
            0x60U,0x32U,0xfeU,0x61U,0xedU,0x66U,0x3dU,0xb7U,0xa3U,0x7fU,0x6fU,0x26U,0x3dU,0x37U,0x0fU,0x69U,
            0xdbU,0x53U,0xa0U,0xdcU,0x0aU,0x17U,0x48U,0xbdU,0xaaU,0xffU,0x62U,0x09U,0xd5U,0x64U,0x54U,0x85U,
            0xe6U,0xe0U,0x01U,0xd1U,0x95U,0x32U,0x55U,0x75U,0x7eU,0x4bU,0x8eU,0x42U,0x81U,0x33U,0x47U,0xb1U,
            0x1dU,0xa6U,0xabU,0x50U,0x0fU,0xd0U,0xacU,0xe7U,0xe6U,0xdfU,0xa3U,0x73U,0x61U,0x99U,0xccU,0xafU,
            0x93U,0x97U,0xedU,0x07U,0x45U,0xa4U,0x27U,0xdcU,0xfaU,0x6cU,0xd6U,0x7bU,0xcbU,0x1aU,0xcfU,0xf3U
        },
        65537UL
    }
};

static const unsigned char tg_known_dh_prime[256] = {
            0xc7U, 0x1cU, 0xaeU, 0xb9U, 0xc6U, 0xb1U, 0xc9U, 0x04U, 0x8eU, 0x6cU, 0x52U, 0x2fU, 0x70U, 0xf1U, 0x3fU, 0x73U,
            0x98U, 0x0dU, 0x40U, 0x23U, 0x8eU, 0x3eU, 0x21U, 0xc1U, 0x49U, 0x34U, 0xd0U, 0x37U, 0x56U, 0x3dU, 0x93U, 0x0fU,
            0x48U, 0x19U, 0x8aU, 0x0aU, 0xa7U, 0xc1U, 0x40U, 0x58U, 0x22U, 0x94U, 0x93U, 0xd2U, 0x25U, 0x30U, 0xf4U, 0xdbU,
            0xfaU, 0x33U, 0x6fU, 0x6eU, 0x0aU, 0xc9U, 0x25U, 0x13U, 0x95U, 0x43U, 0xaeU, 0xd4U, 0x4cU, 0xceU, 0x7cU, 0x37U,
            0x20U, 0xfdU, 0x51U, 0xf6U, 0x94U, 0x58U, 0x70U, 0x5aU, 0xc6U, 0x8cU, 0xd4U, 0xfeU, 0x6bU, 0x6bU, 0x13U, 0xabU,
            0xdcU, 0x97U, 0x46U, 0x51U, 0x29U, 0x69U, 0x32U, 0x84U, 0x54U, 0xf1U, 0x8fU, 0xafU, 0x8cU, 0x59U, 0x5fU, 0x64U,
            0x24U, 0x77U, 0xfeU, 0x96U, 0xbbU, 0x2aU, 0x94U, 0x1dU, 0x5bU, 0xcdU, 0x1dU, 0x4aU, 0xc8U, 0xccU, 0x49U, 0x88U,
            0x07U, 0x08U, 0xfaU, 0x9bU, 0x37U, 0x8eU, 0x3cU, 0x4fU, 0x3aU, 0x90U, 0x60U, 0xbeU, 0xe6U, 0x7cU, 0xf9U, 0xa4U,
            0xa4U, 0xa6U, 0x95U, 0x81U, 0x10U, 0x51U, 0x90U, 0x7eU, 0x16U, 0x27U, 0x53U, 0xb5U, 0x6bU, 0x0fU, 0x6bU, 0x41U,
            0x0dU, 0xbaU, 0x74U, 0xd8U, 0xa8U, 0x4bU, 0x2aU, 0x14U, 0xb3U, 0x14U, 0x4eU, 0x0eU, 0xf1U, 0x28U, 0x47U, 0x54U,
            0xfdU, 0x17U, 0xedU, 0x95U, 0x0dU, 0x59U, 0x65U, 0xb4U, 0xb9U, 0xddU, 0x46U, 0x58U, 0x2dU, 0xb1U, 0x17U, 0x8dU,
            0x16U, 0x9cU, 0x6bU, 0xc4U, 0x65U, 0xb0U, 0xd6U, 0xffU, 0x9cU, 0xa3U, 0x92U, 0x8fU, 0xefU, 0x5bU, 0x9aU, 0xe4U,
            0xe4U, 0x18U, 0xfcU, 0x15U, 0xe8U, 0x3eU, 0xbeU, 0xa0U, 0xf8U, 0x7fU, 0xa9U, 0xffU, 0x5eU, 0xedU, 0x70U, 0x05U,
            0x0dU, 0xedU, 0x28U, 0x49U, 0xf4U, 0x7bU, 0xf9U, 0x59U, 0xd9U, 0x56U, 0x85U, 0x0cU, 0xe9U, 0x29U, 0x85U, 0x1fU,
            0x0dU, 0x81U, 0x15U, 0xf6U, 0x35U, 0xb1U, 0x05U, 0xeeU, 0x2eU, 0x4eU, 0x15U, 0xd0U, 0x4bU, 0x24U, 0x54U, 0xbfU,
            0x6fU, 0x4fU, 0xadU, 0xf0U, 0x34U, 0xb1U, 0x04U, 0x03U, 0x11U, 0x9cU, 0xd8U, 0xe3U, 0xb9U, 0x2fU, 0xccU, 0x5bU
};

static unsigned char tg_xtime(unsigned char x)
{
    return (unsigned char)((x << 1) ^ (((x >> 7) & 1U) * 0x1bU));
}

static void tg_aes_key_expansion(const unsigned char key[32],
                                 unsigned char round_key[240])
{
    unsigned int i;
    unsigned char temp[4];

    memcpy(round_key, key, 32U);
    for (i = 8U; i < 60U; ++i) {
        temp[0] = round_key[(i - 1U) * 4U + 0U];
        temp[1] = round_key[(i - 1U) * 4U + 1U];
        temp[2] = round_key[(i - 1U) * 4U + 2U];
        temp[3] = round_key[(i - 1U) * 4U + 3U];
        if ((i % 8U) == 0U) {
            unsigned char t;
            t = temp[0];
            temp[0] = (unsigned char)(tg_aes_sbox[temp[1]] ^ tg_aes_rcon[i / 8U]);
            temp[1] = tg_aes_sbox[temp[2]];
            temp[2] = tg_aes_sbox[temp[3]];
            temp[3] = tg_aes_sbox[t];
        } else if ((i % 8U) == 4U) {
            temp[0] = tg_aes_sbox[temp[0]];
            temp[1] = tg_aes_sbox[temp[1]];
            temp[2] = tg_aes_sbox[temp[2]];
            temp[3] = tg_aes_sbox[temp[3]];
        }
        round_key[i * 4U + 0U] =
            (unsigned char)(round_key[(i - 8U) * 4U + 0U] ^ temp[0]);
        round_key[i * 4U + 1U] =
            (unsigned char)(round_key[(i - 8U) * 4U + 1U] ^ temp[1]);
        round_key[i * 4U + 2U] =
            (unsigned char)(round_key[(i - 8U) * 4U + 2U] ^ temp[2]);
        round_key[i * 4U + 3U] =
            (unsigned char)(round_key[(i - 8U) * 4U + 3U] ^ temp[3]);
    }
}

static void tg_aes_add_round_key(unsigned char state[16],
                                 const unsigned char *round_key)
{
    unsigned int i;

    for (i = 0U; i < 16U; ++i) {
        state[i] ^= round_key[i];
    }
}

static void tg_aes_sub_bytes(unsigned char state[16])
{
    unsigned int i;

    for (i = 0U; i < 16U; ++i) {
        state[i] = tg_aes_sbox[state[i]];
    }
}

static void tg_aes_shift_rows(unsigned char state[16])
{
    unsigned char tmp;

    tmp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = tmp;
    tmp = state[2]; state[2] = state[10]; state[10] = tmp; tmp = state[6]; state[6] = state[14]; state[14] = tmp;
    tmp = state[3]; state[3] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = tmp;
}

static void tg_aes_mix_columns(unsigned char state[16])
{
    unsigned int i;

    for (i = 0U; i < 4U; ++i) {
        unsigned char *c;
        unsigned char t;
        unsigned char u;
        c = state + (i * 4U);
        t = (unsigned char)(c[0] ^ c[1] ^ c[2] ^ c[3]);
        u = c[0];
        c[0] ^= (unsigned char)(t ^ tg_xtime((unsigned char)(c[0] ^ c[1])));
        c[1] ^= (unsigned char)(t ^ tg_xtime((unsigned char)(c[1] ^ c[2])));
        c[2] ^= (unsigned char)(t ^ tg_xtime((unsigned char)(c[2] ^ c[3])));
        c[3] ^= (unsigned char)(t ^ tg_xtime((unsigned char)(c[3] ^ u)));
    }
}

static unsigned char tg_aes_inverse_sbox(unsigned char value)
{
    unsigned int i;

    for (i = 0U; i < 256U; ++i) {
        if (tg_aes_sbox[i] == value) {
            return (unsigned char)i;
        }
    }
    return 0U;
}

static unsigned char tg_aes_gf_mul(unsigned char a, unsigned char b)
{
    unsigned char result;
    unsigned char high_bit;
    unsigned int i;

    result = 0U;
    for (i = 0U; i < 8U; ++i) {
        if ((b & 1U) != 0U) {
            result ^= a;
        }
        high_bit = (unsigned char)(a & 0x80U);
        a <<= 1;
        if (high_bit != 0U) {
            a ^= 0x1bU;
        }
        b >>= 1;
    }
    return result;
}

static void tg_aes_inv_sub_bytes(unsigned char state[16])
{
    unsigned int i;

    for (i = 0U; i < 16U; ++i) {
        state[i] = tg_aes_inverse_sbox(state[i]);
    }
}

static void tg_aes_inv_shift_rows(unsigned char state[16])
{
    unsigned char tmp;

    tmp = state[13]; state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = tmp;
    tmp = state[2]; state[2] = state[10]; state[10] = tmp; tmp = state[6]; state[6] = state[14]; state[14] = tmp;
    tmp = state[3]; state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = tmp;
}

static void tg_aes_inv_mix_columns(unsigned char state[16])
{
    unsigned int i;

    for (i = 0U; i < 4U; ++i) {
        unsigned char *c;
        unsigned char a0;
        unsigned char a1;
        unsigned char a2;
        unsigned char a3;
        c = state + (i * 4U);
        a0 = c[0];
        a1 = c[1];
        a2 = c[2];
        a3 = c[3];
        c[0] = (unsigned char)(tg_aes_gf_mul(a0, 0x0eU) ^
                               tg_aes_gf_mul(a1, 0x0bU) ^
                               tg_aes_gf_mul(a2, 0x0dU) ^
                               tg_aes_gf_mul(a3, 0x09U));
        c[1] = (unsigned char)(tg_aes_gf_mul(a0, 0x09U) ^
                               tg_aes_gf_mul(a1, 0x0eU) ^
                               tg_aes_gf_mul(a2, 0x0bU) ^
                               tg_aes_gf_mul(a3, 0x0dU));
        c[2] = (unsigned char)(tg_aes_gf_mul(a0, 0x0dU) ^
                               tg_aes_gf_mul(a1, 0x09U) ^
                               tg_aes_gf_mul(a2, 0x0eU) ^
                               tg_aes_gf_mul(a3, 0x0bU));
        c[3] = (unsigned char)(tg_aes_gf_mul(a0, 0x0bU) ^
                               tg_aes_gf_mul(a1, 0x0dU) ^
                               tg_aes_gf_mul(a2, 0x09U) ^
                               tg_aes_gf_mul(a3, 0x0eU));
    }
}

static void tg_aes256_encrypt_block(const unsigned char in[16],
                                    unsigned char out[16],
                                    const unsigned char key[32])
{
    unsigned char state[16];
    unsigned char round_key[240];
    unsigned int round;

    memcpy(state, in, 16U);
    tg_aes_key_expansion(key, round_key);
    tg_aes_add_round_key(state, round_key);
    for (round = 1U; round < 14U; ++round) {
        tg_aes_sub_bytes(state);
        tg_aes_shift_rows(state);
        tg_aes_mix_columns(state);
        tg_aes_add_round_key(state, round_key + (round * 16U));
    }
    tg_aes_sub_bytes(state);
    tg_aes_shift_rows(state);
    tg_aes_add_round_key(state, round_key + 224U);
    memcpy(out, state, 16U);
}

static void tg_aes256_decrypt_block(const unsigned char in[16],
                                    unsigned char out[16],
                                    const unsigned char key[32])
{
    unsigned char state[16];
    unsigned char round_key[240];
    int round;

    memcpy(state, in, 16U);
    tg_aes_key_expansion(key, round_key);
    tg_aes_add_round_key(state, round_key + 224U);
    for (round = 13; round >= 1; --round) {
        tg_aes_inv_shift_rows(state);
        tg_aes_inv_sub_bytes(state);
        tg_aes_add_round_key(state, round_key + ((unsigned int)round * 16U));
        tg_aes_inv_mix_columns(state);
    }
    tg_aes_inv_shift_rows(state);
    tg_aes_inv_sub_bytes(state);
    tg_aes_add_round_key(state, round_key);
    memcpy(out, state, 16U);
}

void tg_mtproto_aes256_ige_encrypt(unsigned char *data,
                                  unsigned long length,
                                  const unsigned char key[32],
                                  const unsigned char iv[32])
{
    unsigned char prev_cipher[16];
    unsigned char prev_plain[16];
    unsigned char block[16];
    unsigned char plain[16];
    unsigned long offset;
    unsigned int i;

    memcpy(prev_cipher, iv, 16U);
    memcpy(prev_plain, iv + 16U, 16U);
    for (offset = 0UL; offset < length; offset += 16UL) {
        memcpy(plain, data + offset, 16U);
        for (i = 0U; i < 16U; ++i) {
            block[i] = (unsigned char)(plain[i] ^ prev_cipher[i]);
        }
        tg_aes256_encrypt_block(block, block, key);
        for (i = 0U; i < 16U; ++i) {
            block[i] = (unsigned char)(block[i] ^ prev_plain[i]);
        }
        memcpy(data + offset, block, 16U);
        memcpy(prev_cipher, block, 16U);
        memcpy(prev_plain, plain, 16U);
    }
}

void tg_mtproto_aes256_ige_decrypt(unsigned char *data,
                                  unsigned long length,
                                  const unsigned char key[32],
                                  const unsigned char iv[32])
{
    unsigned char prev_cipher[16];
    unsigned char prev_plain[16];
    unsigned char block[16];
    unsigned char cipher[16];
    unsigned long offset;
    unsigned int i;

    memcpy(prev_cipher, iv, 16U);
    memcpy(prev_plain, iv + 16U, 16U);
    for (offset = 0UL; offset < length; offset += 16UL) {
        memcpy(cipher, data + offset, 16U);
        for (i = 0U; i < 16U; ++i) {
            block[i] = (unsigned char)(cipher[i] ^ prev_plain[i]);
        }
        tg_aes256_decrypt_block(block, block, key);
        for (i = 0U; i < 16U; ++i) {
            block[i] = (unsigned char)(block[i] ^ prev_cipher[i]);
        }
        memcpy(data + offset, block, 16U);
        memcpy(prev_cipher, cipher, 16U);
        memcpy(prev_plain, block, 16U);
    }
}

static int tg_big_cmp(const unsigned char *a, const unsigned char *b)
{
    unsigned int i;

    for (i = 0U; i < TG_MTPROTO_RSA_MODULUS_LENGTH; ++i) {
        if (a[i] < b[i]) {
            return -1;
        }
        if (a[i] > b[i]) {
            return 1;
        }
    }
    return 0;
}

static void tg_big_sub(unsigned char *a, const unsigned char *b)
{
    int i;
    unsigned int borrow;

    borrow = 0U;
    for (i = (int)TG_MTPROTO_RSA_MODULUS_LENGTH - 1; i >= 0; --i) {
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

static void tg_big_add_mod(unsigned char *a,
                           const unsigned char *b,
                           const unsigned char *modulus)
{
    int i;
    unsigned int carry;
    unsigned char original[TG_MTPROTO_RSA_MODULUS_LENGTH];

    memcpy(original, a, sizeof(original));
    carry = 0U;
    for (i = (int)TG_MTPROTO_RSA_MODULUS_LENGTH - 1; i >= 0; --i) {
        unsigned int sum;
        sum = (unsigned int)a[i] + (unsigned int)b[i] + carry;
        a[i] = (unsigned char)(sum & 0xffU);
        carry = sum >> 8;
    }
    if (carry != 0U || tg_big_cmp(a, modulus) >= 0) {
        if (carry != 0U && tg_big_cmp(original, a) > 0) {
            tg_big_sub(a, modulus);
        } else {
            tg_big_sub(a, modulus);
        }
    }
}

static int tg_big_bit(const unsigned char *a, unsigned int bit)
{
    unsigned int byte_index;
    unsigned int bit_index;

    byte_index = TG_MTPROTO_RSA_MODULUS_LENGTH - 1U - (bit / 8U);
    bit_index = bit % 8U;
    return (a[byte_index] & (1U << bit_index)) != 0U;
}

static void tg_big_mod_mul(const unsigned char *a,
                           const unsigned char *b,
                           const unsigned char *modulus,
                           unsigned char out[TG_MTPROTO_RSA_MODULUS_LENGTH])
{
    unsigned char result[TG_MTPROTO_RSA_MODULUS_LENGTH];
    unsigned char addend[TG_MTPROTO_RSA_MODULUS_LENGTH];
    unsigned int bit;

    memset(result, 0, sizeof(result));
    memcpy(addend, a, sizeof(addend));
    while (tg_big_cmp(addend, modulus) >= 0) {
        tg_big_sub(addend, modulus);
    }
    for (bit = 0U; bit < TG_MTPROTO_RSA_MODULUS_LENGTH * 8U; ++bit) {
        if (tg_big_bit(b, bit)) {
            tg_big_add_mod(result, addend, modulus);
        }
        tg_big_add_mod(addend, addend, modulus);
    }
    memcpy(out, result, TG_MTPROTO_RSA_MODULUS_LENGTH);
}

static void tg_rsa_public_encrypt_raw(
    const unsigned char input[TG_MTPROTO_RSA_MODULUS_LENGTH],
    const tg_mtproto_public_key *key,
    unsigned char output[TG_MTPROTO_RSA_MODULUS_LENGTH])
{
    unsigned char result[TG_MTPROTO_RSA_MODULUS_LENGTH];
    unsigned char base[TG_MTPROTO_RSA_MODULUS_LENGTH];
    unsigned long exponent;

    memset(result, 0, sizeof(result));
    result[TG_MTPROTO_RSA_MODULUS_LENGTH - 1U] = 1U;
    memcpy(base, input, sizeof(base));
    exponent = key->exponent;
    while (exponent > 0UL) {
        if ((exponent & 1UL) != 0UL) {
            tg_big_mod_mul(result, base, key->modulus, result);
        }
        exponent >>= 1;
        if (exponent > 0UL) {
            tg_big_mod_mul(base, base, key->modulus, base);
        }
    }
    memcpy(output, result, TG_MTPROTO_RSA_MODULUS_LENGTH);
}

static void tg_big_mod_exp_bytes(
    const unsigned char base[TG_MTPROTO_DH_VALUE_MAX],
    const unsigned char exponent[TG_MTPROTO_DH_VALUE_MAX],
    const unsigned char modulus[TG_MTPROTO_DH_VALUE_MAX],
    unsigned char output[TG_MTPROTO_DH_VALUE_MAX])
{
    unsigned char result[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char power[TG_MTPROTO_DH_VALUE_MAX];
    unsigned int bit;

    memset(result, 0, sizeof(result));
    result[TG_MTPROTO_DH_VALUE_MAX - 1U] = 1U;
    memcpy(power, base, sizeof(power));
    while (tg_big_cmp(power, modulus) >= 0) {
        tg_big_sub(power, modulus);
    }

    for (bit = 0U; bit < TG_MTPROTO_DH_VALUE_MAX * 8U; ++bit) {
        if (tg_big_bit(exponent, bit)) {
            tg_big_mod_mul(result, power, modulus, result);
        }
        tg_big_mod_mul(power, power, modulus, power);
    }

    memcpy(output, result, TG_MTPROTO_DH_VALUE_MAX);
}

static unsigned long tg_trim_leading_zeroes(const unsigned char *data,
                                            unsigned long data_length)
{
    unsigned long offset;

    offset = 0UL;
    while (offset + 1UL < data_length && data[offset] == 0U) {
        ++offset;
    }
    return offset;
}

static int tg_big_greater_than_one(const unsigned char *value,
                                   unsigned long value_length)
{
    unsigned long i;

    if (value == 0 || value_length == 0) {
        return 0;
    }
    for (i = 0UL; i + 1UL < value_length; ++i) {
        if (value[i] != 0U) {
            return 1;
        }
    }
    return value[value_length - 1UL] > 1U;
}

static int tg_big_less_than_prime_minus_one(const unsigned char *value,
                                            unsigned long value_length,
                                            const unsigned char *prime)
{
    unsigned char limit[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char padded[TG_MTPROTO_DH_VALUE_MAX];

    if (value == 0 || prime == 0 || value_length == 0 ||
        value_length > TG_MTPROTO_DH_VALUE_MAX) {
        return 0;
    }
    memcpy(limit, prime, sizeof(limit));
    limit[TG_MTPROTO_DH_VALUE_MAX - 1U] =
        (unsigned char)(limit[TG_MTPROTO_DH_VALUE_MAX - 1U] - 1U);
    memset(padded, 0, sizeof(padded));
    memcpy(padded + TG_MTPROTO_DH_VALUE_MAX - value_length, value,
           (size_t)value_length);
    return tg_big_cmp(padded, limit) < 0;
}

static int tg_big_within_dh_public_range(const unsigned char *value,
                                         unsigned long value_length,
                                         const unsigned char *prime)
{
    unsigned char lower[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char upper[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char padded[TG_MTPROTO_DH_VALUE_MAX];

    if (value == 0 || prime == 0 || value_length == 0 ||
        value_length > TG_MTPROTO_DH_VALUE_MAX) {
        return 0;
    }
    memset(lower, 0, sizeof(lower));
    lower[7] = 1U;
    memcpy(upper, prime, sizeof(upper));
    tg_big_sub(upper, lower);
    memset(padded, 0, sizeof(padded));
    memcpy(padded + TG_MTPROTO_DH_VALUE_MAX - value_length, value,
           (size_t)value_length);
    return tg_big_cmp(padded, lower) >= 0 && tg_big_cmp(padded, upper) <= 0;
}

const tg_mtproto_public_key *tg_mtproto_builtin_public_keys(
    unsigned int *count)
{
    if (count != 0) {
        *count = (unsigned int)(sizeof(tg_builtin_keys) /
                                sizeof(tg_builtin_keys[0]));
    }
    return tg_builtin_keys;
}

const tg_mtproto_public_key *tg_mtproto_select_public_key(
    const tg_mtproto_res_pq *res_pq)
{
    unsigned int key_count;
    unsigned int i;
    unsigned int j;
    const tg_mtproto_public_key *keys;

    if (res_pq == 0) {
        return 0;
    }
    keys = tg_mtproto_builtin_public_keys(&key_count);
    for (i = 0U; i < res_pq->fingerprint_count; ++i) {
        for (j = 0U; j < key_count; ++j) {
            if (res_pq->fingerprints[i].hi == keys[j].fingerprint.hi &&
                res_pq->fingerprints[i].lo == keys[j].fingerprint.lo) {
                return &keys[j];
            }
        }
    }
    return 0;
}

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
    long dc_id)
{
    tg_mtproto_tl_status status;

    if (nonce == 0 || server_nonce == 0 || new_nonce == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, 0xa9f55f95UL);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, pq, pq_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, p, p_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, q, q_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, nonce, 16UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, server_nonce, 16UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, new_nonce, 32UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u32(writer, (unsigned long)dc_id);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_rsa_pad(
    const unsigned char *data,
    unsigned long data_length,
    const unsigned char random_padding[96],
    const unsigned char temp_key[32],
    const tg_mtproto_public_key *public_key,
    unsigned char encrypted_data[TG_MTPROTO_RSA_PADDED_LENGTH])
{
    unsigned char data_with_padding[TG_RSA_DATA_PADDED_LENGTH];
    unsigned char data_with_hash[TG_RSA_HASHED_LENGTH];
    unsigned char hash_input[32U + TG_RSA_DATA_PADDED_LENGTH];
    unsigned char digest[TG_MTPROTO_SHA256_LENGTH];
    unsigned char iv[32];
    unsigned int i;

    if (data == 0 || random_padding == 0 || temp_key == 0 ||
        public_key == 0 || encrypted_data == 0 ||
        data_length > 144UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }

    memcpy(data_with_padding, data, (size_t)data_length);
    memcpy(data_with_padding + data_length, random_padding,
           (size_t)(TG_RSA_DATA_PADDED_LENGTH - data_length));
    for (i = 0U; i < TG_RSA_DATA_PADDED_LENGTH; ++i) {
        data_with_hash[i] = data_with_padding[TG_RSA_DATA_PADDED_LENGTH - 1U - i];
    }
    memcpy(hash_input, temp_key, 32U);
    memcpy(hash_input + 32U, data_with_padding, sizeof(data_with_padding));
    tg_mtproto_sha256(hash_input, sizeof(hash_input), digest);
    memcpy(data_with_hash + TG_RSA_DATA_PADDED_LENGTH, digest,
           TG_MTPROTO_SHA256_LENGTH);

    memset(iv, 0, sizeof(iv));
    tg_mtproto_aes256_ige_encrypt(data_with_hash, sizeof(data_with_hash), temp_key, iv);
    tg_mtproto_sha256(data_with_hash, sizeof(data_with_hash), digest);
    for (i = 0U; i < 32U; ++i) {
        encrypted_data[i] = (unsigned char)(temp_key[i] ^ digest[i]);
    }
    memcpy(encrypted_data + 32U, data_with_hash, sizeof(data_with_hash));

    if (tg_big_cmp(encrypted_data, public_key->modulus) >= 0) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    tg_rsa_public_encrypt_raw(encrypted_data, public_key, encrypted_data);
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_build_req_dh_params(
    tg_mtproto_tl_writer *writer,
    const unsigned char nonce[16],
    const unsigned char server_nonce[16],
    const unsigned char *p,
    unsigned long p_length,
    const unsigned char *q,
    unsigned long q_length,
    const tg_mtproto_fingerprint *fingerprint,
    const unsigned char encrypted_data[TG_MTPROTO_RSA_PADDED_LENGTH])
{
    tg_mtproto_tl_status status;

    if (nonce == 0 || server_nonce == 0 || fingerprint == 0 ||
        encrypted_data == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer, 0xd712e4beUL);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, nonce, 16UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, server_nonce, 16UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, p, p_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, q, q_length);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(writer, fingerprint->hi,
                                         fingerprint->lo);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, encrypted_data,
                                           TG_MTPROTO_RSA_PADDED_LENGTH);
    }
    return status;
}

static void tg_mtproto_dh_tmp_aes(const unsigned char new_nonce[32],
                                  const unsigned char server_nonce[16],
                                  unsigned char key[32],
                                  unsigned char iv[32])
{
    unsigned char input[64];
    unsigned char hash1[TG_MTPROTO_SHA1_LENGTH];
    unsigned char hash2[TG_MTPROTO_SHA1_LENGTH];
    unsigned char hash3[TG_MTPROTO_SHA1_LENGTH];

    memcpy(input, new_nonce, 32U);
    memcpy(input + 32U, server_nonce, 16U);
    tg_mtproto_sha1(input, 48UL, hash1);

    memcpy(input, server_nonce, 16U);
    memcpy(input + 16U, new_nonce, 32U);
    tg_mtproto_sha1(input, 48UL, hash2);

    memcpy(input, new_nonce, 32U);
    memcpy(input + 32U, new_nonce, 32U);
    tg_mtproto_sha1(input, 64UL, hash3);

    memcpy(key, hash1, 20U);
    memcpy(key + 20U, hash2, 12U);

    memcpy(iv, hash2 + 12U, 8U);
    memcpy(iv + 8U, hash3, 20U);
    memcpy(iv + 28U, new_nonce, 4U);
}

tg_mtproto_tl_status tg_mtproto_parse_server_dh_params_ok(
    const unsigned char *payload,
    unsigned long payload_length,
    tg_mtproto_server_dh_params_ok *out)
{
    tg_mtproto_tl_reader reader;
    const unsigned char *raw;
    const unsigned char *encrypted_answer;
    unsigned long auth_hi;
    unsigned long auth_lo;
    unsigned long msg_hi;
    unsigned long msg_lo;
    unsigned long message_length;
    unsigned long constructor;
    unsigned long encrypted_answer_length;

    if (payload == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, payload, payload_length);
    if (tg_mtproto_tl_read_u64(&reader, &auth_hi, &auth_lo) !=
            TG_MTPROTO_TL_OK ||
        auth_hi != 0UL || auth_lo != 0UL ||
        tg_mtproto_tl_read_u64(&reader, &msg_hi, &msg_lo) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &message_length) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (message_length > payload_length || payload_length - 20UL < message_length) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(&reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_SERVER_DH_PARAMS_OK_CONSTRUCTOR ||
        tg_mtproto_tl_read_raw(&reader, &raw, 16UL) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    memcpy(out->nonce, raw, 16U);
    if (tg_mtproto_tl_read_raw(&reader, &raw, 16UL) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    memcpy(out->server_nonce, raw, 16U);
    if (tg_mtproto_tl_read_bytes(&reader, &encrypted_answer,
                                 &encrypted_answer_length) !=
        TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    if (encrypted_answer_length == 0UL ||
        encrypted_answer_length > sizeof(out->encrypted_answer) ||
        (encrypted_answer_length % 16UL) != 0UL) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    memcpy(out->encrypted_answer, encrypted_answer,
           (size_t)encrypted_answer_length);
    out->encrypted_answer_length = encrypted_answer_length;
    if (reader.offset != 20UL + message_length) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

static tg_mtproto_tl_status tg_mtproto_parse_server_dh_inner_data(
    const unsigned char *data,
    unsigned long data_length,
    tg_mtproto_server_dh_inner_data *out,
    unsigned long *parsed_length)
{
    tg_mtproto_tl_reader reader;
    const unsigned char *raw;
    const unsigned char *bytes;
    unsigned long length;
    unsigned long constructor;

    if (data == 0 || out == 0 || parsed_length == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, data, data_length);
    if (tg_mtproto_tl_read_u32(&reader, &constructor) != TG_MTPROTO_TL_OK ||
        constructor != TG_SERVER_DH_INNER_DATA_CONSTRUCTOR ||
        tg_mtproto_tl_read_raw(&reader, &raw, 16UL) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    memcpy(out->nonce, raw, 16U);
    if (tg_mtproto_tl_read_raw(&reader, &raw, 16UL) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    memcpy(out->server_nonce, raw, 16U);
    if (tg_mtproto_tl_read_u32(&reader, &out->g) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_bytes(&reader, &bytes, &length) !=
            TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    if (length == 0UL || length > sizeof(out->dh_prime)) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    memcpy(out->dh_prime, bytes, (size_t)length);
    out->dh_prime_length = length;
    if (tg_mtproto_tl_read_bytes(&reader, &bytes, &length) !=
        TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    if (length == 0UL || length > sizeof(out->g_a)) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    memcpy(out->g_a, bytes, (size_t)length);
    out->g_a_length = length;
    if (tg_mtproto_tl_read_u32(&reader, &out->server_time) !=
        TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    *parsed_length = reader.offset;
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_decrypt_server_dh_inner_data(
    const unsigned char *encrypted_answer,
    unsigned long encrypted_answer_length,
    const unsigned char new_nonce[32],
    const unsigned char expected_nonce[16],
    const unsigned char expected_server_nonce[16],
    tg_mtproto_server_dh_inner_data *out)
{
    unsigned char decrypted[TG_MTPROTO_DH_ENCRYPTED_ANSWER_MAX];
    unsigned char key[32];
    unsigned char iv[32];
    unsigned char digest[TG_MTPROTO_SHA1_LENGTH];
    unsigned long parsed_length;
    tg_mtproto_tl_status status;

    if (encrypted_answer == 0 || new_nonce == 0 || expected_nonce == 0 ||
        expected_server_nonce == 0 || out == 0 ||
        encrypted_answer_length < 36UL ||
        encrypted_answer_length > sizeof(decrypted) ||
        (encrypted_answer_length % 16UL) != 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }

    memcpy(decrypted, encrypted_answer, (size_t)encrypted_answer_length);
    tg_mtproto_dh_tmp_aes(new_nonce, expected_server_nonce, key, iv);
    tg_mtproto_aes256_ige_decrypt(decrypted, encrypted_answer_length, key, iv);

    status = tg_mtproto_parse_server_dh_inner_data(decrypted + 20U,
                                                   encrypted_answer_length - 20UL,
                                                   out, &parsed_length);
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }
    tg_mtproto_sha1(decrypted + 20U, parsed_length, digest);
    if (memcmp(decrypted, digest, TG_MTPROTO_SHA1_LENGTH) != 0 ||
        memcmp(out->nonce, expected_nonce, 16U) != 0 ||
        memcmp(out->server_nonce, expected_server_nonce, 16U) != 0 ||
        out->g < 2UL || out->g > 7UL) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }

    return TG_MTPROTO_TL_OK;
}

int tg_mtproto_check_dh_params(const tg_mtproto_server_dh_inner_data *inner)
{
    if (inner == 0 ||
        inner->g < 2UL || inner->g > 7UL ||
        inner->dh_prime_length != TG_MTPROTO_DH_VALUE_MAX ||
        memcmp(inner->dh_prime, tg_known_dh_prime,
               TG_MTPROTO_DH_VALUE_MAX) != 0 ||
        !tg_big_greater_than_one(inner->g_a, inner->g_a_length) ||
        !tg_big_less_than_prime_minus_one(inner->g_a, inner->g_a_length,
                                          inner->dh_prime) ||
        !tg_big_within_dh_public_range(inner->g_a, inner->g_a_length,
                                       inner->dh_prime)) {
        return 0;
    }
    return 1;
}

tg_mtproto_tl_status tg_mtproto_build_set_client_dh_params(
    tg_mtproto_tl_writer *writer,
    const unsigned char nonce[16],
    const unsigned char server_nonce[16],
    const unsigned char *encrypted_data,
    unsigned long encrypted_data_length)
{
    tg_mtproto_tl_status status;

    if (nonce == 0 || server_nonce == 0 || encrypted_data == 0 ||
        encrypted_data_length == 0UL ||
        encrypted_data_length > TG_MTPROTO_DH_ENCRYPTED_ANSWER_MAX ||
        (encrypted_data_length % 16UL) != 0UL) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    status = tg_mtproto_tl_write_u32(writer,
                                     TG_SET_CLIENT_DH_PARAMS_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, nonce, 16UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(writer, server_nonce, 16UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(writer, encrypted_data,
                                           encrypted_data_length);
    }
    return status;
}

tg_mtproto_tl_status tg_mtproto_build_client_dh_request(
    const tg_mtproto_server_dh_inner_data *inner,
    const unsigned char new_nonce[32],
    const unsigned char b[TG_MTPROTO_DH_VALUE_MAX],
    const unsigned char padding[15],
    unsigned char encrypted_data[TG_MTPROTO_DH_ENCRYPTED_ANSWER_MAX],
    unsigned long *encrypted_data_length,
    unsigned char auth_key[TG_MTPROTO_AUTH_KEY_LENGTH])
{
    unsigned char base[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char g_a[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char g_b[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char inner_data[360];
    unsigned char data_with_hash[TG_MTPROTO_DH_ENCRYPTED_ANSWER_MAX];
    unsigned char key[32];
    unsigned char iv[32];
    unsigned char digest[TG_MTPROTO_SHA1_LENGTH];
    unsigned long g_b_offset;
    unsigned long inner_length;
    unsigned long total_length;
    unsigned long pad_length;
    tg_mtproto_tl_writer writer;
    tg_mtproto_tl_status status;

    if (inner == 0 || new_nonce == 0 || b == 0 || padding == 0 ||
        encrypted_data == 0 || encrypted_data_length == 0 ||
        auth_key == 0 || !tg_mtproto_check_dh_params(inner)) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }

    memset(base, 0, sizeof(base));
    base[TG_MTPROTO_DH_VALUE_MAX - 1U] = (unsigned char)inner->g;
    tg_big_mod_exp_bytes(base, b, inner->dh_prime, g_b);

    memset(g_a, 0, sizeof(g_a));
    memcpy(g_a + TG_MTPROTO_DH_VALUE_MAX - inner->g_a_length, inner->g_a,
           (size_t)inner->g_a_length);
    tg_big_mod_exp_bytes(g_a, b, inner->dh_prime, auth_key);

    g_b_offset = tg_trim_leading_zeroes(g_b, sizeof(g_b));
    tg_mtproto_tl_writer_init(&writer, inner_data, sizeof(inner_data));
    status = tg_mtproto_tl_write_u32(&writer,
                                     TG_CLIENT_DH_INNER_DATA_CONSTRUCTOR);
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(&writer, inner->nonce, 16UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_raw(&writer, inner->server_nonce, 16UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_u64(&writer, 0UL, 0UL);
    }
    if (status == TG_MTPROTO_TL_OK) {
        status = tg_mtproto_tl_write_bytes(
            &writer, g_b + g_b_offset,
            (unsigned long)(sizeof(g_b) - g_b_offset));
    }
    if (status != TG_MTPROTO_TL_OK) {
        return status;
    }

    inner_length = writer.length;
    total_length = TG_MTPROTO_SHA1_LENGTH + inner_length;
    pad_length = (16UL - (total_length % 16UL)) % 16UL;
    if (pad_length > 15UL ||
        total_length + pad_length > TG_MTPROTO_DH_ENCRYPTED_ANSWER_MAX) {
        return TG_MTPROTO_TL_BUFFER_TOO_SMALL;
    }

    tg_mtproto_sha1(inner_data, inner_length, digest);
    memcpy(data_with_hash, digest, TG_MTPROTO_SHA1_LENGTH);
    memcpy(data_with_hash + TG_MTPROTO_SHA1_LENGTH, inner_data,
           (size_t)inner_length);
    if (pad_length > 0UL) {
        memcpy(data_with_hash + total_length, padding, (size_t)pad_length);
    }
    total_length += pad_length;
    tg_mtproto_dh_tmp_aes(new_nonce, inner->server_nonce, key, iv);
    tg_mtproto_aes256_ige_encrypt(data_with_hash, total_length, key, iv);
    memcpy(encrypted_data, data_with_hash, (size_t)total_length);
    *encrypted_data_length = total_length;
    return TG_MTPROTO_TL_OK;
}

tg_mtproto_tl_status tg_mtproto_parse_set_client_dh_answer(
    const unsigned char *payload,
    unsigned long payload_length,
    tg_mtproto_set_client_dh_answer *out)
{
    tg_mtproto_tl_reader reader;
    const unsigned char *raw;
    unsigned long auth_hi;
    unsigned long auth_lo;
    unsigned long msg_hi;
    unsigned long msg_lo;
    unsigned long message_length;
    unsigned long constructor;

    if (payload == 0 || out == 0) {
        return TG_MTPROTO_TL_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    tg_mtproto_tl_reader_init(&reader, payload, payload_length);
    if (tg_mtproto_tl_read_u64(&reader, &auth_hi, &auth_lo) !=
            TG_MTPROTO_TL_OK ||
        auth_hi != 0UL || auth_lo != 0UL ||
        tg_mtproto_tl_read_u64(&reader, &msg_hi, &msg_lo) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_read_u32(&reader, &message_length) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (message_length != 52UL ||
        message_length > payload_length ||
        payload_length - 20UL < message_length) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    if (tg_mtproto_tl_read_u32(&reader, &constructor) != TG_MTPROTO_TL_OK ||
        (constructor != TG_DH_GEN_OK_CONSTRUCTOR &&
         constructor != TG_DH_GEN_RETRY_CONSTRUCTOR &&
         constructor != TG_DH_GEN_FAIL_CONSTRUCTOR)) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    out->constructor = constructor;
    if (tg_mtproto_tl_read_raw(&reader, &raw, 16UL) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    memcpy(out->nonce, raw, 16U);
    if (tg_mtproto_tl_read_raw(&reader, &raw, 16UL) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    memcpy(out->server_nonce, raw, 16U);
    if (tg_mtproto_tl_read_raw(&reader, &raw, 16UL) != TG_MTPROTO_TL_OK) {
        return TG_MTPROTO_TL_TRUNCATED;
    }
    memcpy(out->new_nonce_hash, raw, 16U);
    if (reader.offset != 20UL + message_length) {
        return TG_MTPROTO_TL_INVALID_DATA;
    }
    return TG_MTPROTO_TL_OK;
}

int tg_mtproto_verify_dh_gen_ok(const tg_mtproto_set_client_dh_answer *answer,
                                const unsigned char expected_nonce[16],
                                const unsigned char expected_server_nonce[16],
                                const unsigned char new_nonce[32],
                                const unsigned char auth_key[TG_MTPROTO_AUTH_KEY_LENGTH])
{
    unsigned char auth_key_hash[TG_MTPROTO_SHA1_LENGTH];
    unsigned char input[41];
    unsigned char digest[TG_MTPROTO_SHA1_LENGTH];

    if (answer == 0 || expected_nonce == 0 || expected_server_nonce == 0 ||
        new_nonce == 0 || auth_key == 0 ||
        answer->constructor != TG_DH_GEN_OK_CONSTRUCTOR ||
        memcmp(answer->nonce, expected_nonce, 16U) != 0 ||
        memcmp(answer->server_nonce, expected_server_nonce, 16U) != 0) {
        return 0;
    }
    tg_mtproto_sha1(auth_key, TG_MTPROTO_AUTH_KEY_LENGTH, auth_key_hash);
    memcpy(input, new_nonce, 32U);
    input[32] = 1U;
    memcpy(input + 33U, auth_key_hash, 8U);
    tg_mtproto_sha1(input, sizeof(input), digest);
    return memcmp(answer->new_nonce_hash, digest + 4U, 16U) == 0;
}

int tg_mtproto_rsa_self_test(void)
{
    static const unsigned char aes_key[32] = {
        0x00U,0x01U,0x02U,0x03U,0x04U,0x05U,0x06U,0x07U,
        0x08U,0x09U,0x0aU,0x0bU,0x0cU,0x0dU,0x0eU,0x0fU,
        0x10U,0x11U,0x12U,0x13U,0x14U,0x15U,0x16U,0x17U,
        0x18U,0x19U,0x1aU,0x1bU,0x1cU,0x1dU,0x1eU,0x1fU
    };
    static const unsigned char aes_plain[16] = {
        0x00U,0x11U,0x22U,0x33U,0x44U,0x55U,0x66U,0x77U,
        0x88U,0x99U,0xaaU,0xbbU,0xccU,0xddU,0xeeU,0xffU
    };
    static const unsigned char aes_expected[16] = {
        0x8eU,0xa2U,0xb7U,0xcaU,0x51U,0x67U,0x45U,0xbfU,
        0xeaU,0xfcU,0x49U,0x90U,0x4bU,0x49U,0x60U,0x89U
    };
    static const unsigned char rsa_two_expected[256] = {
        0x55U,0xe0U,0x55U,0x42U,0xd2U,0x73U,0xf9U,0x0fU,0x31U,0x39U,0x60U,0xe2U,0xceU,0xcfU,0x6dU,0xa1U,
        0xe6U,0xfcU,0x5dU,0x3dU,0xd3U,0xa0U,0x74U,0x4eU,0xf3U,0x71U,0x8bU,0x6dU,0x40U,0xa8U,0x45U,0x68U,
        0x82U,0x9eU,0xdcU,0xaeU,0x8aU,0x91U,0x8dU,0x23U,0x88U,0xbaU,0x05U,0x5cU,0xaeU,0x3bU,0x32U,0x12U,
        0xc6U,0x9dU,0x3dU,0x30U,0x68U,0x56U,0x56U,0xdaU,0x3fU,0x52U,0xfcU,0x2aU,0xd6U,0xeeU,0xf2U,0xabU,
        0xe1U,0x4fU,0xe1U,0xfaU,0x6dU,0x9eU,0xeaU,0x67U,0x5fU,0x43U,0xa4U,0x67U,0x57U,0xfbU,0x83U,0x57U,
        0x45U,0x42U,0x76U,0xeeU,0x19U,0x9aU,0xebU,0x60U,0x48U,0xa4U,0x8dU,0xf0U,0x48U,0xb2U,0x4cU,0x7dU,
        0x90U,0x4cU,0x8dU,0x33U,0x54U,0xc0U,0x74U,0xdfU,0x4fU,0x05U,0x57U,0x85U,0xf8U,0x6cU,0xc2U,0x6aU,
        0x48U,0xf8U,0x16U,0xfdU,0xcfU,0x66U,0xefU,0x69U,0x29U,0x94U,0x22U,0x84U,0x88U,0xceU,0xd1U,0x5bU,
        0x7eU,0x66U,0xeaU,0x09U,0x3bU,0x89U,0xb7U,0xe9U,0x12U,0x3cU,0x09U,0xbcU,0x46U,0x63U,0xb8U,0xe1U,
        0xe9U,0x1dU,0xccU,0x37U,0x02U,0x7aU,0x9bU,0x64U,0xd2U,0x21U,0xe7U,0xeaU,0xe7U,0x34U,0x8dU,0xdcU,
        0x5eU,0x70U,0x40U,0x6cU,0xd7U,0xbdU,0xdcU,0x55U,0x8eU,0x0cU,0x0cU,0x7dU,0x50U,0x19U,0x65U,0xa1U,
        0xfaU,0x5dU,0x57U,0x8aU,0x5dU,0x4eU,0xd8U,0x94U,0x8cU,0xecU,0x4eU,0xf6U,0x69U,0xc2U,0xc2U,0x75U,
        0xdaU,0xceU,0x5cU,0xd1U,0x77U,0xf1U,0x71U,0x2fU,0xebU,0xa0U,0xaaU,0x0bU,0x2eU,0xa9U,0x0aU,0x08U,
        0x61U,0x20U,0xd4U,0xe6U,0x93U,0x74U,0x28U,0x45U,0x11U,0x45U,0x42U,0xf2U,0xd3U,0xc4U,0x07U,0xa0U,
        0x49U,0x0bU,0xddU,0x22U,0xf8U,0x1bU,0x4eU,0x8cU,0xc8U,0x63U,0x2eU,0xbdU,0x8aU,0x9cU,0x47U,0x81U,
        0x9dU,0xc4U,0xe9U,0x86U,0x4eU,0x71U,0xddU,0xc7U,0x48U,0x9bU,0xdbU,0x86U,0x3eU,0x5dU,0x9fU,0x4dU
    };
    unsigned char block[16];
    unsigned char rsa_input[256];
    unsigned char rsa_output[256];
    unsigned char inner[128];
    unsigned char encrypted[256];
    unsigned char req[384];
    unsigned char answer[128];
    unsigned char answer_with_hash[144];
    unsigned char client_encrypted[TG_MTPROTO_DH_ENCRYPTED_ANSWER_MAX];
    unsigned char auth_key[TG_MTPROTO_AUTH_KEY_LENGTH];
    unsigned char b[TG_MTPROTO_DH_VALUE_MAX];
    unsigned char client_padding[15];
    unsigned char final_hash_input[41];
    unsigned char auth_key_hash[TG_MTPROTO_SHA1_LENGTH];
    unsigned char padding[96];
    unsigned char temp_key[32];
    unsigned char tmp_aes_key[32];
    unsigned char iv[32];
    unsigned char digest[TG_MTPROTO_SHA1_LENGTH];
    unsigned long body_length;
    unsigned long client_encrypted_length;
    tg_mtproto_server_dh_params_ok params_ok;
    tg_mtproto_server_dh_inner_data inner_data;
    tg_mtproto_server_dh_inner_data dh_check;
    tg_mtproto_set_client_dh_answer dh_answer;
    tg_mtproto_tl_writer writer;
    unsigned int key_count;
    unsigned int i;
    const tg_mtproto_public_key *keys;

    memcpy(block, aes_plain, sizeof(block));
    tg_aes256_encrypt_block(block, block, aes_key);
    if (memcmp(block, aes_expected, sizeof(block)) != 0) {
        return 2;
    }

    keys = tg_mtproto_builtin_public_keys(&key_count);
    if (key_count != 1U ||
        keys[0].fingerprint.hi != 0xd09d1d85UL ||
        keys[0].fingerprint.lo != 0xde64fd85UL ||
        keys[0].exponent != 65537UL) {
        return 2;
    }

    memset(rsa_input, 0, sizeof(rsa_input));
    rsa_input[255] = 2U;
    tg_rsa_public_encrypt_raw(rsa_input, keys, rsa_output);
    if (memcmp(rsa_output, rsa_two_expected, sizeof(rsa_two_expected)) != 0) {
        return 2;
    }

    for (i = 0U; i < sizeof(padding); ++i) {
        padding[i] = (unsigned char)i;
    }
    for (i = 0U; i < sizeof(temp_key); ++i) {
        temp_key[i] = (unsigned char)(0xa0U + i);
    }

    tg_mtproto_tl_writer_init(&writer, inner, sizeof(inner));
    if (tg_mtproto_build_p_q_inner_data_dc(&writer,
                                           keys[0].modulus, 8UL,
                                           keys[0].modulus, 4UL,
                                           keys[0].modulus + 4UL, 4UL,
                                           keys[0].modulus, keys[0].modulus + 16U,
                                           temp_key, 2L) != TG_MTPROTO_TL_OK ||
        writer.length != 100UL) {
        return 2;
    }
    for (i = 0U; i < 16U; ++i) {
        temp_key[0] = (unsigned char)(0xa0U + i);
        if (tg_mtproto_rsa_pad(inner, writer.length, padding, temp_key, keys,
                               encrypted) == TG_MTPROTO_TL_OK) {
            break;
        }
    }
    if (i == 16U) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, req, sizeof(req));
    if (tg_mtproto_build_req_dh_params(&writer, keys[0].modulus,
                                       keys[0].modulus + 16U,
                                       keys[0].modulus, 4UL,
                                       keys[0].modulus + 4U, 4UL,
                                       &keys[0].fingerprint, encrypted) !=
            TG_MTPROTO_TL_OK ||
        writer.length != 320UL ||
        req[0] != 0xbeU || req[1] != 0xe4U ||
        req[2] != 0x12U || req[3] != 0xd7U) {
        return 2;
    }

    tg_mtproto_tl_writer_init(&writer, answer, sizeof(answer));
    if (tg_mtproto_tl_write_u32(&writer, TG_SERVER_DH_INNER_DATA_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_raw(&writer, keys[0].modulus, 16UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_raw(&writer, keys[0].modulus + 16U, 16UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 3UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_bytes(&writer, keys[0].modulus, 16UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_bytes(&writer, keys[0].modulus + 32U, 16UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 0x6777e5ebUL) !=
            TG_MTPROTO_TL_OK) {
        return 2;
    }
    tg_mtproto_sha1(answer, writer.length, digest);
    memcpy(answer_with_hash, digest, TG_MTPROTO_SHA1_LENGTH);
    memcpy(answer_with_hash + TG_MTPROTO_SHA1_LENGTH, answer, writer.length);
    memset(answer_with_hash + TG_MTPROTO_SHA1_LENGTH + writer.length, 0,
           sizeof(answer_with_hash) - TG_MTPROTO_SHA1_LENGTH - writer.length);
    tg_mtproto_dh_tmp_aes(temp_key, keys[0].modulus + 16U, tmp_aes_key, iv);
    tg_mtproto_aes256_ige_encrypt(answer_with_hash, sizeof(answer_with_hash),
                          tmp_aes_key, iv);
    tg_mtproto_tl_writer_init(&writer, req, sizeof(req));
    if (tg_mtproto_tl_write_u64(&writer, 0UL, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x6777e5ebUL, 0x00059764UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, 184UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, TG_SERVER_DH_PARAMS_OK_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_raw(&writer, keys[0].modulus, 16UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_raw(&writer, keys[0].modulus + 16U, 16UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_bytes(&writer, answer_with_hash,
                                  sizeof(answer_with_hash)) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_server_dh_params_ok(req, writer.length, &params_ok) !=
            TG_MTPROTO_TL_OK ||
        params_ok.encrypted_answer_length != sizeof(answer_with_hash) ||
        tg_mtproto_decrypt_server_dh_inner_data(
            params_ok.encrypted_answer, params_ok.encrypted_answer_length,
            temp_key, keys[0].modulus, keys[0].modulus + 16U, &inner_data) !=
            TG_MTPROTO_TL_OK ||
        inner_data.g != 3UL ||
        inner_data.dh_prime_length != 16UL ||
        inner_data.g_a_length != 16UL ||
        inner_data.server_time != 0x6777e5ebUL) {
        return 2;
    }

    memset(&dh_check, 0, sizeof(dh_check));
    memcpy(dh_check.nonce, keys[0].modulus, 16U);
    memcpy(dh_check.server_nonce, keys[0].modulus + 16U, 16U);
    dh_check.g = 3UL;
    memcpy(dh_check.dh_prime, tg_known_dh_prime, sizeof(tg_known_dh_prime));
    dh_check.dh_prime_length = sizeof(tg_known_dh_prime);
    dh_check.g_a[7] = 1U;
    dh_check.g_a_length = sizeof(dh_check.g_a);
    dh_check.server_time = 0x6777e5ebUL;
    memset(b, 0, sizeof(b));
    b[sizeof(b) - 1U] = 3U;
    for (i = 0U; i < sizeof(client_padding); ++i) {
        client_padding[i] = (unsigned char)(0x55U + i);
    }
    memset(auth_key, 0, sizeof(auth_key));
    if (!tg_mtproto_check_dh_params(&dh_check) ||
        tg_mtproto_build_client_dh_request(&dh_check, temp_key, b,
                                           client_padding, client_encrypted,
                                           &client_encrypted_length,
                                           auth_key) != TG_MTPROTO_TL_OK ||
        client_encrypted_length == 0UL ||
        (client_encrypted_length % 16UL) != 0UL ||
        !tg_big_greater_than_one(auth_key, sizeof(auth_key))) {
        return 2;
    }
    tg_mtproto_tl_writer_init(&writer, req, sizeof(req));
    if (tg_mtproto_build_set_client_dh_params(
            &writer, dh_check.nonce, dh_check.server_nonce,
            client_encrypted, client_encrypted_length) != TG_MTPROTO_TL_OK ||
        writer.length < 40UL ||
        req[0] != 0x1fU || req[1] != 0x5fU ||
        req[2] != 0x04U || req[3] != 0xf5U) {
        return 2;
    }

    tg_mtproto_sha1(auth_key, sizeof(auth_key), auth_key_hash);
    memcpy(final_hash_input, temp_key, 32U);
    final_hash_input[32] = 1U;
    memcpy(final_hash_input + 33U, auth_key_hash, 8U);
    tg_mtproto_sha1(final_hash_input, sizeof(final_hash_input), digest);
    tg_mtproto_tl_writer_init(&writer, answer, sizeof(answer));
    if (tg_mtproto_tl_write_u32(&writer, TG_DH_GEN_OK_CONSTRUCTOR) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_raw(&writer, dh_check.nonce, 16UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_raw(&writer, dh_check.server_nonce, 16UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_raw(&writer, digest + 4U, 16UL) !=
            TG_MTPROTO_TL_OK) {
        return 2;
    }
    body_length = writer.length;
    tg_mtproto_tl_writer_init(&writer, req, sizeof(req));
    if (tg_mtproto_tl_write_u64(&writer, 0UL, 0UL) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u64(&writer, 0x6777e5ebUL, 0x00059768UL) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_u32(&writer, body_length) != TG_MTPROTO_TL_OK ||
        tg_mtproto_tl_write_raw(&writer, answer, body_length) !=
            TG_MTPROTO_TL_OK ||
        tg_mtproto_parse_set_client_dh_answer(req, writer.length,
                                              &dh_answer) !=
            TG_MTPROTO_TL_OK ||
        !tg_mtproto_verify_dh_gen_ok(&dh_answer, dh_check.nonce,
                                     dh_check.server_nonce, temp_key,
                                     auth_key)) {
        return 2;
    }

    return 0;
}
