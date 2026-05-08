/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_BOT_H
#define TG_BOT_H

#include "tg_https.h"
#include "tg_telegram.h"

/**
 * Bot API orchestration result.
 *
 * TELEGRAM_ERROR means the HTTP response was received but could not be parsed as
 * a Telegram response; telegram_status contains the lower-level reason.
 * HTTPS_ERROR means the live transport failed before a parseable HTTP response
 * was available.
 */
typedef enum tg_bot_status {
    TG_BOT_OK = 0,
    TG_BOT_INVALID_ARGUMENT = 1,
    TG_BOT_TOKEN_ERROR = 2,
    TG_BOT_PATH_ERROR = 3,
    TG_BOT_HTTPS_ERROR = 4,
    TG_BOT_TELEGRAM_ERROR = 5
} tg_bot_status;

/**
 * Detail outputs produced by tg_bot_get_me_from_token_file().
 *
 * The status fields are valid only when the corresponding tg_bot_status points
 * to that layer. response contains borrowed views into the caller-owned HTTP
 * buffer passed to tg_bot_get_me_from_token_file().
 */
typedef struct tg_bot_call_result {
    tg_file_status file_status;
    tg_telegram_status telegram_status;
    tg_https_status https_status;
    tg_tls_status tls_status;
    tg_net_status net_status;
    tg_http_parse_status http_parse_status;
    tg_json_status json_status;
    tg_telegram_http_response response;
} tg_bot_call_result;

/**
 * Initializes a call result to default OK/empty values.
 */
void tg_bot_call_result_init(tg_bot_call_result *result);

/**
 * Parses a complete HTTP response as the result of Telegram getMe.
 *
 * This is the offline-testable half of the Bot API layer. http_response remains
 * owned by the caller and must stay valid while result->response is used.
 */
tg_bot_status tg_bot_parse_get_me_http_response(const char *http_response,
                                                unsigned long http_response_length,
                                                tg_bot_call_result *result);

/**
 * Loads a token from file and performs getMe using HTTPS GET.
 *
 * http_buffer is caller-owned and receives the raw HTTP response. This function
 * does not print or expose the token. Live TLS behavior depends on the platform
 * backend and should be tested carefully on MorphOS.
 */
tg_bot_status tg_bot_get_me_from_token_file(const char *token_file_path,
                                            char *http_buffer,
                                            unsigned long http_buffer_size,
                                            unsigned long *http_response_length,
                                            tg_bot_call_result *result,
                                            char *error_buffer,
                                            unsigned long error_buffer_size);

/**
 * Returns a static string for status. The caller must not free it.
 */
const char *tg_bot_status_name(tg_bot_status status);

#endif
