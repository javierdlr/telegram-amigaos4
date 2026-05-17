/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_MTPROTO_CRYPTO_H
#define TG_MTPROTO_CRYPTO_H

#define TG_MTPROTO_SHA1_LENGTH 20
#define TG_MTPROTO_SHA256_LENGTH 32

void tg_mtproto_sha1(const unsigned char *data, unsigned long data_length,
                     unsigned char digest[TG_MTPROTO_SHA1_LENGTH]);
void tg_mtproto_sha256(const unsigned char *data, unsigned long data_length,
                       unsigned char digest[TG_MTPROTO_SHA256_LENGTH]);
int tg_mtproto_crypto_self_test(void);

#endif
