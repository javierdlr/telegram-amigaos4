/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include "tg_telegram.h"

static void tg_telegram_response_init(tg_telegram_response *response)
{
    response->ok = 0;
    response->has_description = 0;
    response->description = 0;
    response->description_length = 0;
    response->has_result = 0;
    response->result.type = TG_JSON_VALUE_NULL;
    response->result.start = 0;
    response->result.length = 0;
    response->result.bool_value = 0;
}

static tg_telegram_status tg_telegram_set_json_status(tg_json_status status,
                                                      tg_json_status *json_status)
{
    if (json_status != 0) {
        *json_status = status;
    }
    return TG_TELEGRAM_JSON_ERROR;
}

tg_telegram_status tg_telegram_parse_response(const char *json, unsigned long json_length,
                                               tg_telegram_response *response,
                                               tg_json_status *json_status)
{
    tg_json_status local_json_status;
    tg_json_value value;

    if (json_status != 0) {
        *json_status = TG_JSON_OK;
    }
    if (response != 0) {
        tg_telegram_response_init(response);
    }
    if (json == 0 || response == 0) {
        return TG_TELEGRAM_INVALID_ARGUMENT;
    }

    /* Every Telegram API response is expected to expose a top-level boolean "ok". */
    local_json_status = tg_json_object_get(json, json_length, "ok", &value);
    if (local_json_status == TG_JSON_NOT_FOUND) {
        if (json_status != 0) {
            *json_status = local_json_status;
        }
        return TG_TELEGRAM_MISSING_OK;
    }
    if (local_json_status != TG_JSON_OK) {
        return tg_telegram_set_json_status(local_json_status, json_status);
    }
    if (value.type != TG_JSON_VALUE_BOOL) {
        return TG_TELEGRAM_TYPE_MISMATCH;
    }
    response->ok = value.bool_value;

    local_json_status = tg_json_object_get(json, json_length, "description", &value);
    if (local_json_status == TG_JSON_OK) {
        if (value.type != TG_JSON_VALUE_STRING) {
            return TG_TELEGRAM_TYPE_MISMATCH;
        }
        response->has_description = 1;
        response->description = value.start;
        response->description_length = value.length;
    } else if (local_json_status != TG_JSON_NOT_FOUND) {
        return tg_telegram_set_json_status(local_json_status, json_status);
    }

    local_json_status = tg_json_object_get(json, json_length, "result", &value);
    if (local_json_status == TG_JSON_OK) {
        response->has_result = 1;
        response->result = value;
    } else if (local_json_status != TG_JSON_NOT_FOUND) {
        return tg_telegram_set_json_status(local_json_status, json_status);
    }

    return TG_TELEGRAM_OK;
}

const char *tg_telegram_status_name(tg_telegram_status status)
{
    switch (status) {
    case TG_TELEGRAM_OK:
        return "ok";
    case TG_TELEGRAM_INVALID_ARGUMENT:
        return "invalid-argument";
    case TG_TELEGRAM_JSON_ERROR:
        return "json-error";
    case TG_TELEGRAM_MISSING_OK:
        return "missing-ok";
    case TG_TELEGRAM_TYPE_MISMATCH:
        return "type-mismatch";
    default:
        return "unknown";
    }
}
