/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_TELEGRAM_H
#define TG_TELEGRAM_H

#include "tg_json.h"

typedef enum tg_telegram_status {
    TG_TELEGRAM_OK = 0,
    TG_TELEGRAM_INVALID_ARGUMENT = 1,
    TG_TELEGRAM_JSON_ERROR = 2,
    TG_TELEGRAM_MISSING_OK = 3,
    TG_TELEGRAM_TYPE_MISMATCH = 4
} tg_telegram_status;

typedef struct tg_telegram_response {
    int ok;
    int has_description;
    const char *description;
    unsigned long description_length;
    int has_result;
    tg_json_value result;
} tg_telegram_response;

tg_telegram_status tg_telegram_parse_response(const char *json, unsigned long json_length,
                                               tg_telegram_response *response,
                                               tg_json_status *json_status);
const char *tg_telegram_status_name(tg_telegram_status status);

#endif
