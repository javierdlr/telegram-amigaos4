/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_MTPROTO_PROBE_H
#define TG_MTPROTO_PROBE_H

#include <stdio.h>

#include "tg_mtproto_tl.h"

tg_mtproto_tl_status tg_mtproto_build_req_pq_multi(
    tg_mtproto_tl_writer *writer,
    unsigned long message_id_hi,
    unsigned long message_id_lo,
    const unsigned char nonce[16]);

int tg_mtproto_req_pq_probe(const char *host, const char *port, FILE *stream);
int tg_mtproto_probe_self_test(void);

#endif
