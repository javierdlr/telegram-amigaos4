/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_telegram.h"

const char *tg_telegram_api_host(void)
{
    return "api.telegram.org";
}

tg_telegram_status tg_telegram_build_bot_path(const char *token, const char *method,
                                              char *path_buffer,
                                              unsigned long path_buffer_size,
                                              unsigned long *path_length)
{
    unsigned long token_length;
    unsigned long method_length;
    unsigned long needed;

    if (path_length != 0) {
        *path_length = 0;
    }
    if (token == 0 || method == 0 || path_buffer == 0 || path_length == 0 ||
        token[0] == '\0' || method[0] == '\0') {
        return TG_TELEGRAM_INVALID_ARGUMENT;
    }

    token_length = (unsigned long)strlen(token);
    method_length = (unsigned long)strlen(method);
    needed = token_length + method_length + 6;

    if (needed + 1 > path_buffer_size) {
        return TG_TELEGRAM_BUFFER_TOO_SMALL;
    }

    /*
     * Official Bot API requests use:
     *   https://api.telegram.org/bot<token>/METHOD_NAME
     * This function builds only the path so the transport layer can stay generic.
     */
    strcpy(path_buffer, "/bot");
    strcat(path_buffer, token);
    strcat(path_buffer, "/");
    strcat(path_buffer, method);
    *path_length = needed;

    return TG_TELEGRAM_OK;
}

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

static void tg_telegram_http_response_init(tg_telegram_http_response *response)
{
    response->http_status_code = 0;
    tg_telegram_response_init(&response->api);
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

tg_telegram_status tg_telegram_parse_http_response(const char *http_response,
                                                   unsigned long http_response_length,
                                                   tg_telegram_http_response *response,
                                                   tg_http_parse_status *http_parse_status,
                                                   tg_json_status *json_status)
{
    tg_http_parse_status local_http_status;
    tg_http_response parsed_http;
    tg_telegram_status telegram_status;

    if (http_parse_status != 0) {
        *http_parse_status = TG_HTTP_PARSE_OK;
    }
    if (json_status != 0) {
        *json_status = TG_JSON_OK;
    }
    if (response != 0) {
        tg_telegram_http_response_init(response);
    }
    if (http_response == 0 || response == 0) {
        return TG_TELEGRAM_INVALID_ARGUMENT;
    }

    local_http_status = tg_http_parse_response(http_response, http_response_length,
                                               &parsed_http);
    if (local_http_status != TG_HTTP_PARSE_OK) {
        if (http_parse_status != 0) {
            *http_parse_status = local_http_status;
        }
        return TG_TELEGRAM_HTTP_PARSE_ERROR;
    }

    response->http_status_code = parsed_http.status_code;
    telegram_status = tg_telegram_parse_response(parsed_http.body, parsed_http.body_length,
                                                 &response->api, json_status);
    return telegram_status;
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
    case TG_TELEGRAM_BUFFER_TOO_SMALL:
        return "buffer-too-small";
    case TG_TELEGRAM_HTTP_PARSE_ERROR:
        return "http-parse-error";
    default:
        return "unknown";
    }
}
