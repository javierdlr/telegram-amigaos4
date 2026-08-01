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

/* Decode a whole baseline JPEG (e.g. the downloaded 160px avatar), picking the
   largest 1/2^k tjpgd scale that fits TG_AVATAR_SRC_MAX, then nearest-scale
   into dst_rgb (dw*dh*3, RGB888). 0 = ok. */
int tg_avatar_decode_jpeg(const unsigned char *jpeg, unsigned long jpeg_len,
                          unsigned char *dst_rgb, int dw, int dh);

/* General message-photo path: decode a baseline JPEG with tjpgd and scale it
   into caller-owned RGB888. Intermediate memory is allocated only for the
   duration of the decode and bounded by source_edge_cap. */
int tg_image_decode_jpeg_scaled(const unsigned char *jpeg,
                                unsigned long jpeg_len,
                                unsigned char *dst_rgb,
                                int dw, int dh,
                                int source_edge_cap);

/* Expand + decode + nearest-neighbour scale into dst_rgb (dw*dh*3, RGB888).
   0 = ok; any failure leaves the caller free to fall back to initials. */
int tg_avatar_decode_stripped(const unsigned char *stripped,
                              unsigned long stripped_len,
                              unsigned char *dst_rgb, int dw, int dh);

int tg_avatar_self_test(void);

#endif
