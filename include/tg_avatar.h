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

/* Resumable variant used by the GUI. The input JPEG and destination RGB buffer
   must remain valid until destroy. Each step decodes at most max_mcus and
   returns 0=more, 1=done, -1=error. ready_rows grows monotonically, allowing
   the caller to repaint completed top-down bands between steps. */
typedef struct tg_image_jpeg_decoder tg_image_jpeg_decoder;
tg_image_jpeg_decoder *tg_image_jpeg_decoder_begin(
    const unsigned char *jpeg, unsigned long jpeg_len,
    unsigned char *dst_rgb, int dw, int dh, int source_edge_cap,
    int *decode_rc);
int tg_image_jpeg_decoder_step(tg_image_jpeg_decoder *decoder,
                               unsigned int max_mcus,
                               int *ready_rows,
                               int *decode_rc);
void tg_image_jpeg_decoder_destroy(tg_image_jpeg_decoder *decoder);

/* Fit source dimensions inside a square canonical edge without changing the
   aspect ratio or enlarging a smaller image. 0 = valid dimensions returned. */
int tg_image_canonical_size(unsigned long source_w,
                            unsigned long source_h,
                            int edge_cap,
                            int *out_w,
                            int *out_h);

/* Apply the 4x4 ordered-dither offset used before palette matching. */
void tg_image_ordered_dither_rgb(const unsigned char *rgb,
                                 int x, int y,
                                 unsigned char *out_rgb);
/* Same Bayer matrix with an explicit amplitude: 4 = full, 2 = light,
   0 = disabled. Values outside 0..4 are clamped. */
void tg_image_ordered_dither_rgb_level(const unsigned char *rgb,
                                       int x, int y, int amplitude,
                                       unsigned char *out_rgb);

/* Expand + decode + nearest-neighbour scale into dst_rgb (dw*dh*3, RGB888).
   0 = ok; any failure leaves the caller free to fall back to initials. */
int tg_avatar_decode_stripped(const unsigned char *stripped,
                              unsigned long stripped_len,
                              unsigned char *dst_rgb, int dw, int dh);

int tg_avatar_self_test(void);

#endif
