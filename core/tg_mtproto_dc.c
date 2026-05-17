/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_mtproto_dc.h"

static const tg_mtproto_dc_option tg_mtproto_dc_options[] = {
    {1, "pluto", "pluto.web.telegram.org"},
    {2, "venus", "venus.web.telegram.org"},
    {3, "aurora", "aurora.web.telegram.org"},
    {4, "vesta", "vesta.web.telegram.org"},
    {5, "flora", "flora.web.telegram.org"}
};

const tg_mtproto_dc_option *tg_mtproto_dc_by_id(int id)
{
    unsigned int i;

    for (i = 0; i < sizeof(tg_mtproto_dc_options) /
                    sizeof(tg_mtproto_dc_options[0]); ++i) {
        if (tg_mtproto_dc_options[i].id == id) {
            return &tg_mtproto_dc_options[i];
        }
    }

    return 0;
}

int tg_mtproto_dc_self_test(void)
{
    const tg_mtproto_dc_option *dc;

    dc = tg_mtproto_dc_by_id(2);
    if (dc == 0 ||
        dc->id != 2 ||
        strcmp(dc->name, "venus") != 0 ||
        strcmp(dc->web_host, "venus.web.telegram.org") != 0) {
        return 2;
    }

    if (tg_mtproto_dc_by_id(0) != 0 ||
        tg_mtproto_dc_by_id(6) != 0) {
        return 2;
    }

    return 0;
}
