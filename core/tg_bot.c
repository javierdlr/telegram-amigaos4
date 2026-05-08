/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include "tg_bot.h"

static tg_bot_status tg_bot_parse_http_response(const char *http_response,
                                                unsigned long http_response_length,
                                                tg_bot_call_result *result)
{
    tg_telegram_status telegram_status;

    if (http_response == 0 || result == 0) {
        return TG_BOT_INVALID_ARGUMENT;
    }

    tg_bot_call_result_init(result);
    telegram_status = tg_telegram_parse_http_response(http_response, http_response_length,
                                                      &result->response,
                                                      &result->http_parse_status,
                                                      &result->json_status);
    result->telegram_status = telegram_status;
    if (telegram_status != TG_TELEGRAM_OK) {
        return TG_BOT_TELEGRAM_ERROR;
    }

    return TG_BOT_OK;
}

static tg_bot_status tg_bot_append_char(char *buffer, unsigned long buffer_size,
                                        unsigned long *position, char c)
{
    if (*position + 1 >= buffer_size) {
        return TG_BOT_BODY_ERROR;
    }
    buffer[*position] = c;
    ++(*position);
    buffer[*position] = '\0';
    return TG_BOT_OK;
}

static tg_bot_status tg_bot_append_text(char *buffer, unsigned long buffer_size,
                                        unsigned long *position, const char *text)
{
    while (*text != '\0') {
        if (tg_bot_append_char(buffer, buffer_size, position, *text) != TG_BOT_OK) {
            return TG_BOT_BODY_ERROR;
        }
        ++text;
    }
    return TG_BOT_OK;
}

static tg_bot_status tg_bot_append_json_hex_escape(char *buffer,
                                                   unsigned long buffer_size,
                                                   unsigned long *position,
                                                   unsigned char c)
{
    static const char hex[] = "0123456789abcdef";

    if (tg_bot_append_text(buffer, buffer_size, position, "\\u00") != TG_BOT_OK) {
        return TG_BOT_BODY_ERROR;
    }
    if (tg_bot_append_char(buffer, buffer_size, position, hex[(c >> 4) & 0x0f]) != TG_BOT_OK) {
        return TG_BOT_BODY_ERROR;
    }
    return tg_bot_append_char(buffer, buffer_size, position, hex[c & 0x0f]);
}

static tg_bot_status tg_bot_append_json_string(char *buffer, unsigned long buffer_size,
                                               unsigned long *position, const char *text)
{
    unsigned char c;

    if (tg_bot_append_char(buffer, buffer_size, position, '"') != TG_BOT_OK) {
        return TG_BOT_BODY_ERROR;
    }

    while (*text != '\0') {
        c = (unsigned char)*text;
        if (c == '"' || c == '\\') {
            if (tg_bot_append_char(buffer, buffer_size, position, '\\') != TG_BOT_OK ||
                tg_bot_append_char(buffer, buffer_size, position, (char)c) != TG_BOT_OK) {
                return TG_BOT_BODY_ERROR;
            }
        } else if (c == '\n') {
            if (tg_bot_append_text(buffer, buffer_size, position, "\\n") != TG_BOT_OK) {
                return TG_BOT_BODY_ERROR;
            }
        } else if (c == '\r') {
            if (tg_bot_append_text(buffer, buffer_size, position, "\\r") != TG_BOT_OK) {
                return TG_BOT_BODY_ERROR;
            }
        } else if (c == '\t') {
            if (tg_bot_append_text(buffer, buffer_size, position, "\\t") != TG_BOT_OK) {
                return TG_BOT_BODY_ERROR;
            }
        } else if (c < 0x20) {
            if (tg_bot_append_json_hex_escape(buffer, buffer_size, position, c) != TG_BOT_OK) {
                return TG_BOT_BODY_ERROR;
            }
        } else {
            if (tg_bot_append_char(buffer, buffer_size, position, (char)c) != TG_BOT_OK) {
                return TG_BOT_BODY_ERROR;
            }
        }
        ++text;
    }

    return tg_bot_append_char(buffer, buffer_size, position, '"');
}

void tg_bot_call_result_init(tg_bot_call_result *result)
{
    if (result == 0) {
        return;
    }
    result->file_status = TG_FILE_OK;
    result->telegram_status = TG_TELEGRAM_OK;
    result->https_status = TG_HTTPS_OK;
    result->tls_status = TG_TLS_OK;
    result->net_status = TG_NET_OK;
    result->http_parse_status = TG_HTTP_PARSE_OK;
    result->json_status = TG_JSON_OK;
    result->response.http_status_code = 0;
    result->response.api.ok = 0;
    result->response.api.has_description = 0;
    result->response.api.description = 0;
    result->response.api.description_length = 0;
    result->response.api.has_result = 0;
    result->response.api.result.type = TG_JSON_VALUE_NULL;
    result->response.api.result.start = 0;
    result->response.api.result.length = 0;
    result->response.api.result.bool_value = 0;
}

tg_bot_status tg_bot_parse_get_me_http_response(const char *http_response,
                                                unsigned long http_response_length,
                                                tg_bot_call_result *result)
{
    return tg_bot_parse_http_response(http_response, http_response_length, result);
}

tg_bot_status tg_bot_parse_send_message_http_response(const char *http_response,
                                                      unsigned long http_response_length,
                                                      tg_bot_call_result *result)
{
    return tg_bot_parse_http_response(http_response, http_response_length, result);
}

tg_bot_status tg_bot_build_send_message_body(const char *chat_id, const char *text,
                                             char *body_buffer,
                                             unsigned long body_buffer_size,
                                             unsigned long *body_length)
{
    unsigned long position;

    if (body_length != 0) {
        *body_length = 0;
    }
    if (body_buffer != 0 && body_buffer_size > 0) {
        body_buffer[0] = '\0';
    }
    if (chat_id == 0 || text == 0 || body_buffer == 0 || body_buffer_size == 0 ||
        body_length == 0 || chat_id[0] == '\0') {
        return TG_BOT_INVALID_ARGUMENT;
    }

    position = 0;
    if (tg_bot_append_text(body_buffer, body_buffer_size, &position,
                           "{\"chat_id\":") != TG_BOT_OK) {
        return TG_BOT_BODY_ERROR;
    }
    if (tg_bot_append_json_string(body_buffer, body_buffer_size, &position,
                                  chat_id) != TG_BOT_OK) {
        return TG_BOT_BODY_ERROR;
    }
    if (tg_bot_append_text(body_buffer, body_buffer_size, &position,
                           ",\"text\":") != TG_BOT_OK) {
        return TG_BOT_BODY_ERROR;
    }
    if (tg_bot_append_json_string(body_buffer, body_buffer_size, &position,
                                  text) != TG_BOT_OK) {
        return TG_BOT_BODY_ERROR;
    }
    if (tg_bot_append_char(body_buffer, body_buffer_size, &position, '}') != TG_BOT_OK) {
        return TG_BOT_BODY_ERROR;
    }

    *body_length = position;
    return TG_BOT_OK;
}

tg_bot_status tg_bot_get_me_from_token_file(const char *token_file_path,
                                            char *http_buffer,
                                            unsigned long http_buffer_size,
                                            unsigned long *http_response_length,
                                            tg_bot_call_result *result,
                                            char *error_buffer,
                                            unsigned long error_buffer_size)
{
    tg_telegram_status telegram_status;
    tg_https_status https_status;
    char token[128];
    char path[256];
    unsigned long token_length;
    unsigned long path_length;

    if (http_response_length != 0) {
        *http_response_length = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    if (token_file_path == 0 || http_buffer == 0 || http_buffer_size == 0 ||
        http_response_length == 0 || result == 0) {
        return TG_BOT_INVALID_ARGUMENT;
    }

    tg_bot_call_result_init(result);

    telegram_status = tg_telegram_load_token_file(token_file_path, token, sizeof(token),
                                                  &token_length, &result->file_status);
    if (telegram_status != TG_TELEGRAM_OK) {
        result->telegram_status = telegram_status;
        return TG_BOT_TOKEN_ERROR;
    }

    telegram_status = tg_telegram_build_bot_path(token, "getMe", path, sizeof(path),
                                                 &path_length);
    if (telegram_status != TG_TELEGRAM_OK) {
        result->telegram_status = telegram_status;
        return TG_BOT_PATH_ERROR;
    }

    https_status = tg_https_get(tg_telegram_api_host(), "443", path,
                                http_buffer, http_buffer_size,
                                http_response_length, &result->tls_status,
                                &result->net_status, error_buffer,
                                error_buffer_size);
    result->https_status = https_status;
    if (https_status != TG_HTTPS_OK) {
        return TG_BOT_HTTPS_ERROR;
    }

    return tg_bot_parse_get_me_http_response(http_buffer, *http_response_length, result);
}

tg_bot_status tg_bot_send_message_from_token_file(const char *token_file_path,
                                                  const char *chat_id,
                                                  const char *text,
                                                  char *http_buffer,
                                                  unsigned long http_buffer_size,
                                                  unsigned long *http_response_length,
                                                  tg_bot_call_result *result,
                                                  char *error_buffer,
                                                  unsigned long error_buffer_size)
{
    tg_telegram_status telegram_status;
    tg_https_status https_status;
    tg_bot_status body_status;
    char token[128];
    char path[256];
    char body[2048];
    unsigned long token_length;
    unsigned long path_length;
    unsigned long body_length;

    if (http_response_length != 0) {
        *http_response_length = 0;
    }
    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }
    if (token_file_path == 0 || chat_id == 0 || text == 0 || http_buffer == 0 ||
        http_buffer_size == 0 || http_response_length == 0 || result == 0) {
        return TG_BOT_INVALID_ARGUMENT;
    }

    tg_bot_call_result_init(result);

    telegram_status = tg_telegram_load_token_file(token_file_path, token, sizeof(token),
                                                  &token_length, &result->file_status);
    if (telegram_status != TG_TELEGRAM_OK) {
        result->telegram_status = telegram_status;
        return TG_BOT_TOKEN_ERROR;
    }

    telegram_status = tg_telegram_build_bot_path(token, "sendMessage", path, sizeof(path),
                                                 &path_length);
    if (telegram_status != TG_TELEGRAM_OK) {
        result->telegram_status = telegram_status;
        return TG_BOT_PATH_ERROR;
    }

    body_status = tg_bot_build_send_message_body(chat_id, text, body, sizeof(body),
                                                 &body_length);
    if (body_status != TG_BOT_OK) {
        return body_status;
    }

    https_status = tg_https_post(tg_telegram_api_host(), "443", path,
                                 "application/json", body, body_length,
                                 http_buffer, http_buffer_size,
                                 http_response_length, &result->tls_status,
                                 &result->net_status, error_buffer,
                                 error_buffer_size);
    result->https_status = https_status;
    if (https_status != TG_HTTPS_OK) {
        return TG_BOT_HTTPS_ERROR;
    }

    return tg_bot_parse_send_message_http_response(http_buffer, *http_response_length,
                                                  result);
}

const char *tg_bot_status_name(tg_bot_status status)
{
    switch (status) {
    case TG_BOT_OK:
        return "ok";
    case TG_BOT_INVALID_ARGUMENT:
        return "invalid-argument";
    case TG_BOT_TOKEN_ERROR:
        return "token-error";
    case TG_BOT_PATH_ERROR:
        return "path-error";
    case TG_BOT_HTTPS_ERROR:
        return "https-error";
    case TG_BOT_TELEGRAM_ERROR:
        return "telegram-error";
    case TG_BOT_BODY_ERROR:
        return "body-error";
    default:
        return "unknown";
    }
}
