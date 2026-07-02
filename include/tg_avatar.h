/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */
#ifndef TG_AVATAR_H
#define TG_AVATAR_H

#include <stdio.h>

/* Largest source thumb we decode (stripped thumbs are ~40 px). */
#define TG_AVATAR_SRC_MAX 64

/* Expands a stripped thumb into a real baseline JPEG (header template + payload
   + FFD9). 0 = ok. */
int tg_avatar_expand_stripped(const unsigned char *stripped,
                              unsigned long stripped_len,
                              unsigned char *out, unsigned long out_cap,
                              unsigned long *out_len);

/* Expand + decode + nearest-neighbour scale into dst_rgb (dw*dh*3, RGB888).
   0 = ok; any failure leaves the caller free to fall back to initials. */
int tg_avatar_decode_stripped(const unsigned char *stripped,
                              unsigned long stripped_len,
                              unsigned char *dst_rgb, int dw, int dh);

int tg_avatar_self_test(void);

#endif
