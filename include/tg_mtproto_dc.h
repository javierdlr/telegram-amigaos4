/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_MTPROTO_DC_H
#define TG_MTPROTO_DC_H

typedef struct tg_mtproto_dc_option {
    int id;
    const char *name;
    const char *web_host;
} tg_mtproto_dc_option;

const tg_mtproto_dc_option *tg_mtproto_dc_by_id(int id);
int tg_mtproto_dc_self_test(void);

#endif
