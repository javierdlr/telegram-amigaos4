/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "tg_app.h"
#include "tg_bot.h"
#include "tg_config.h"
#include "tg_file.h"
#include "tg_https.h"
#include "tg_http.h"
#include "tg_json.h"
#include "tg_log.h"
#include "tg_net.h"
#include "tg_platform.h"
#include "tg_telegram.h"

#define TG_STATE_MAX_UPDATES 5UL

typedef enum tg_read_output_mode {
    TG_READ_OUTPUT_STATE = 0,
    TG_READ_OUTPUT_INBOX = 1
} tg_read_output_mode;

static const char tg_default_token_file_name[] = "telegram-token.txt";
static const char tg_default_offset_file_name[] = "telegram-offset.txt";
static const char tg_default_inbox_log_file_name[] = "telegram-inbox.log";
static const char tg_default_chat_state_file_name[] = "telegram-chats.txt";
static const char tg_default_poll_seconds_text[] = "5";
static const char tg_default_max_iterations_text[] = "10";

static int tg_run_http_test(const tg_config *config)
{
    tg_http_status http_status;
    tg_http_parse_status parse_status;
    tg_http_response parsed_response;
    tg_net_status net_status;
    char net_error[128];
    char response[2048];
    unsigned long response_len;

    http_status = tg_http_get(config->http_test_host, config->http_test_port,
                              config->http_test_path, response, sizeof(response),
                              &response_len, &net_status, net_error, sizeof(net_error));
    if (http_status != TG_HTTP_OK) {
        printf("http test: failed: %s", tg_http_status_name(http_status));
        if (http_status == TG_HTTP_NET_ERROR) {
            printf(" / %s", tg_net_status_name(net_status));
        }
        if (net_error[0] != '\0') {
            printf(" (%s)", net_error);
        }
        printf("\n");
        return 2;
    }

    printf("http test: %s:%s%s ok, received %lu bytes\n",
           config->http_test_host, config->http_test_port,
           config->http_test_path, response_len);

    parse_status = tg_http_parse_response(response, response_len, &parsed_response);
    if (parse_status != TG_HTTP_PARSE_OK) {
        printf("http parse: failed: %s\n", tg_http_parse_status_name(parse_status));
        printf("%s\n", response);
        return 0;
    }

    printf("http status: %d", parsed_response.status_code);
    if (parsed_response.reason_length > 0) {
        printf(" %.*s", (int)parsed_response.reason_length, parsed_response.reason);
    }
    printf("\n");
    printf("http body: %lu bytes\n", parsed_response.body_length);
    printf("%.*s\n", (int)parsed_response.body_length, parsed_response.body);
    return 0;
}

static int tg_run_https_test(const tg_config *config)
{
    tg_https_status https_status;
    tg_http_parse_status parse_status;
    tg_http_response parsed_response;
    tg_tls_status tls_status;
    tg_net_status net_status;
    char net_error[128];
    char response[2048];
    unsigned long response_len;

    https_status = tg_https_get(config->https_test_host, config->https_test_port,
                                config->https_test_path, response, sizeof(response),
                                &response_len, &tls_status, &net_status,
                                net_error, sizeof(net_error));
    if (https_status != TG_HTTPS_OK) {
        printf("https test: failed: %s", tg_https_status_name(https_status));
        if (https_status == TG_HTTPS_TLS_ERROR) {
            printf(" / %s", tg_tls_status_name(tls_status));
            if (tls_status == TG_TLS_NET_ERROR) {
                printf(" / %s", tg_net_status_name(net_status));
            }
        }
        if (net_error[0] != '\0') {
            printf(" (%s)", net_error);
        }
        printf("\n");
        return 2;
    }

    printf("https test: %s:%s%s ok, received %lu bytes\n",
           config->https_test_host, config->https_test_port,
           config->https_test_path, response_len);

    parse_status = tg_http_parse_response(response, response_len, &parsed_response);
    if (parse_status != TG_HTTP_PARSE_OK) {
        printf("https parse: failed: %s\n", tg_http_parse_status_name(parse_status));
        printf("%s\n", response);
        return 0;
    }

    printf("https status: %d", parsed_response.status_code);
    if (parsed_response.reason_length > 0) {
        printf(" %.*s", (int)parsed_response.reason_length, parsed_response.reason);
    }
    printf("\n");
    printf("https body: %lu bytes\n", parsed_response.body_length);
    printf("%.*s\n", (int)parsed_response.body_length, parsed_response.body);
    return 0;
}

static int tg_run_http_post_self_test(void)
{
    static const char body[] = "{\"chat_id\":123,\"text\":\"Hello from Amiga\"}";
    char request[512];
    char length_header[64];
    unsigned long request_length;
    unsigned long body_length;
    tg_http_status http_status;

    body_length = (unsigned long)strlen(body);
    http_status = tg_http_build_post_request("api.telegram.org",
                                             "/botTOKEN/sendMessage",
                                             "application/json",
                                             body, body_length,
                                             request, sizeof(request),
                                             &request_length);
    if (http_status != TG_HTTP_OK) {
        printf("http post self-test: failed: %s\n", tg_http_status_name(http_status));
        return 2;
    }

    sprintf(length_header, "Content-Length: %lu\r\n", body_length);
    if (strncmp(request, "POST /botTOKEN/sendMessage HTTP/1.0\r\n",
                strlen("POST /botTOKEN/sendMessage HTTP/1.0\r\n")) != 0 ||
        strstr(request, "Host: api.telegram.org\r\n") == 0 ||
        strstr(request, "Content-Type: application/json\r\n") == 0 ||
        strstr(request, length_header) == 0 ||
        request_length < body_length ||
        strcmp(request + request_length - body_length, body) != 0) {
        puts("http post self-test: failed: request mismatch");
        printf("%s\n", request);
        return 2;
    }

    printf("http post self-test: ok request, %lu bytes\n", request_length);
    printf("http post body: %s\n", request + request_length - body_length);
    return 0;
}

static int tg_run_json_test(const tg_config *config)
{
    tg_json_status json_status;
    tg_json_value value;
    unsigned long json_length;

    json_length = (unsigned long)strlen(config->json_test_input);
    json_status = tg_json_object_get(config->json_test_input, json_length,
                                     config->json_test_field, &value);
    if (json_status != TG_JSON_OK) {
        printf("json test: failed: %s\n", tg_json_status_name(json_status));
        return 2;
    }

    printf("json field: %s\n", config->json_test_field);
    printf("json type: %s\n", tg_json_value_type_name(value.type));

    if (value.type == TG_JSON_VALUE_BOOL) {
        printf("json value: %s\n", value.bool_value ? "true" : "false");
    } else if (value.type == TG_JSON_VALUE_STRING ||
               value.type == TG_JSON_VALUE_NUMBER ||
               value.type == TG_JSON_VALUE_OBJECT ||
               value.type == TG_JSON_VALUE_ARRAY) {
        printf("json value: %.*s\n", (int)value.length, value.start);
    } else {
        printf("json value: null\n");
    }

    return 0;
}

static void tg_print_telegram_response(const tg_telegram_response *response)
{
    printf("telegram ok: %s\n", response->ok ? "true" : "false");
    if (response->has_description) {
        printf("telegram description: %.*s\n",
               (int)response->description_length, response->description);
    }
    if (response->has_result) {
        printf("telegram result type: %s\n",
               tg_json_value_type_name(response->result.type));
        if (response->result.type != TG_JSON_VALUE_NULL &&
            response->result.start != 0) {
            printf("telegram result: %.*s\n",
                   (int)response->result.length, response->result.start);
        }
    }
}

static void tg_print_update_summary(const tg_bot_update_summary *update)
{
    tg_json_status json_status;
    char decoded_text[512];
    unsigned long decoded_length;

    if (!update->has_update) {
        puts("telegram update: none");
        return;
    }

    printf("telegram update id: %s\n", update->update_id);
    if (!update->has_message) {
        puts("telegram update message: none");
        return;
    }

    printf("telegram update chat id: %s\n", update->chat_id);
    if (update->has_text) {
        json_status = tg_json_string_decode(update->text, update->text_length,
                                            decoded_text, sizeof(decoded_text),
                                            &decoded_length);
        if (json_status == TG_JSON_OK) {
            printf("telegram update text: %s\n", decoded_text);
        } else {
            printf("telegram update text: <decode failed: %s>\n",
                   tg_json_status_name(json_status));
        }
    } else {
        puts("telegram update text: none");
    }
}

static int tg_app_append_char(char *buffer, unsigned long buffer_size,
                              unsigned long *position, char c)
{
    if (*position + 1 >= buffer_size) {
        return 1;
    }
    buffer[*position] = c;
    ++(*position);
    buffer[*position] = '\0';
    return 0;
}

static int tg_app_append_text(char *buffer, unsigned long buffer_size,
                              unsigned long *position, const char *text)
{
    while (*text != '\0') {
        if (tg_app_append_char(buffer, buffer_size, position, *text) != 0) {
            return 1;
        }
        ++text;
    }
    return 0;
}

static int tg_app_append_n(char *buffer, unsigned long buffer_size,
                           unsigned long *position, const char *text,
                           unsigned long text_length)
{
    unsigned long index;

    for (index = 0; index < text_length; ++index) {
        if (tg_app_append_char(buffer, buffer_size, position, text[index]) != 0) {
            return 1;
        }
    }
    return 0;
}

static int tg_is_leap_year(unsigned long year)
{
    return (year % 4UL == 0UL) &&
           ((year % 100UL != 0UL) || (year % 400UL == 0UL));
}

static unsigned long tg_days_in_year(unsigned long year)
{
    return tg_is_leap_year(year) ? 366UL : 365UL;
}

static unsigned long tg_days_in_month(unsigned long year, unsigned long month)
{
    static const unsigned long month_days[] = {
        31UL, 28UL, 31UL, 30UL, 31UL, 30UL,
        31UL, 31UL, 30UL, 31UL, 30UL, 31UL
    };

    if (month == 2UL && tg_is_leap_year(year)) {
        return 29UL;
    }
    return month_days[month - 1UL];
}

static int tg_format_unix_utc(unsigned long timestamp,
                              char *buffer, unsigned long buffer_size)
{
    unsigned long seconds;
    unsigned long minutes;
    unsigned long hours;
    unsigned long days;
    unsigned long year;
    unsigned long month;
    unsigned long month_days;

    seconds = timestamp % 60UL;
    timestamp /= 60UL;
    minutes = timestamp % 60UL;
    timestamp /= 60UL;
    hours = timestamp % 24UL;
    days = timestamp / 24UL;

    year = 1970UL;
    while (days >= tg_days_in_year(year)) {
        days -= tg_days_in_year(year);
        ++year;
    }

    month = 1UL;
    for (;;) {
        month_days = tg_days_in_month(year, month);
        if (days < month_days) {
            break;
        }
        days -= month_days;
        ++month;
    }

    if (buffer_size < 24UL) {
        return 1;
    }
    sprintf(buffer, "%04lu-%02lu-%02lu %02lu:%02lu:%02lu UTC",
            year, month, days + 1UL, hours, minutes, seconds);
    return 0;
}

static int tg_format_inbox_date(const tg_bot_update_summary *update,
                                char *buffer, unsigned long buffer_size)
{
    unsigned long timestamp;
    char *end_ptr;
    unsigned long position;

    if (buffer == 0 || buffer_size == 0) {
        return 1;
    }
    buffer[0] = '\0';
    if (update == 0 || !update->has_date || update->date[0] == '\0') {
        position = 0;
        return tg_app_append_text(buffer, buffer_size, &position, "unknown");
    }

    end_ptr = 0;
    timestamp = strtoul(update->date, &end_ptr, 10);
    if (end_ptr == update->date || *end_ptr != '\0') {
        position = 0;
        return tg_app_append_text(buffer, buffer_size, &position, update->date);
    }

    if (tg_format_unix_utc(timestamp, buffer, buffer_size) != 0) {
        position = 0;
        return tg_app_append_text(buffer, buffer_size, &position, update->date);
    }

    return 0;
}

static void tg_sanitize_one_line(char *text)
{
    while (*text != '\0') {
        if (*text == '\r' || *text == '\n' || *text == '\t') {
            *text = ' ';
        }
        ++text;
    }
}

static void tg_sanitize_chat_field(char *text)
{
    while (*text != '\0') {
        if (*text == '\r' || *text == '\n' || *text == '\t' || *text == '|') {
            *text = ' ';
        }
        ++text;
    }
}

static int tg_inbox_content_text(const tg_bot_update_summary *update,
                                 char *buffer, unsigned long buffer_size)
{
    tg_json_status json_status;
    unsigned long decoded_length;
    unsigned long position;

    if (buffer == 0 || buffer_size == 0 || update == 0) {
        return 1;
    }
    buffer[0] = '\0';

    if (update->has_text) {
        json_status = tg_json_string_decode(update->text, update->text_length,
                                            buffer, buffer_size,
                                            &decoded_length);
        if (json_status != TG_JSON_OK) {
            position = 0;
            return tg_app_append_text(buffer, buffer_size, &position,
                                      "<decode failed>");
        }
        tg_sanitize_one_line(buffer);
        return 0;
    }

    position = 0;
    if (update->message_kind == TG_BOT_MESSAGE_PHOTO) {
        return tg_app_append_text(buffer, buffer_size, &position, "<photo>");
    } else if (update->message_kind == TG_BOT_MESSAGE_STICKER) {
        return tg_app_append_text(buffer, buffer_size, &position, "<sticker>");
    } else if (update->message_kind == TG_BOT_MESSAGE_DOCUMENT) {
        return tg_app_append_text(buffer, buffer_size, &position, "<document>");
    } else if (update->message_kind == TG_BOT_MESSAGE_UNSUPPORTED) {
        return tg_app_append_text(buffer, buffer_size, &position,
                                  "<unsupported message>");
    }

    return tg_app_append_text(buffer, buffer_size, &position, "<no message>");
}

static int tg_build_inbox_line(const tg_bot_update_summary *update,
                               char *buffer, unsigned long buffer_size)
{
    char date_text[64];
    char content_text[512];
    const char *sender;
    unsigned long position;

    if (update == 0 || buffer == 0 || buffer_size == 0 || !update->has_update) {
        return 1;
    }

    if (tg_format_inbox_date(update, date_text, sizeof(date_text)) != 0 ||
        tg_inbox_content_text(update, content_text, sizeof(content_text)) != 0) {
        return 1;
    }
    sender = update->has_sender_name ? update->sender_name : "unknown";

    position = 0;
    buffer[0] = '\0';
    if (tg_app_append_text(buffer, buffer_size, &position, date_text) != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, " | ") != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, update->update_id) != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, " | chat ") != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, update->chat_id) != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, " | ") != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, sender) != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, " | ") != 0 ||
        tg_app_append_text(buffer, buffer_size, &position,
                           tg_bot_message_kind_name(update->message_kind)) != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, " | ") != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, content_text) != 0) {
        return 1;
    }

    return 0;
}

static int tg_append_inbox_log_line(const char *path,
                                    const tg_bot_update_summary *update)
{
    tg_file_status file_status;
    char line[1024];
    unsigned long position;

    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    if (tg_build_inbox_line(update, line, sizeof(line)) != 0) {
        puts("inbox log: line too long");
        return 2;
    }

    position = (unsigned long)strlen(line);
    if (tg_app_append_char(line, sizeof(line), &position, '\n') != 0) {
        puts("inbox log: line too long");
        return 2;
    }

    file_status = tg_file_append_text(path, line, position);
    if (file_status != TG_FILE_OK) {
        printf("inbox log: write failed: %s\n",
               tg_file_status_name(file_status));
        return 2;
    }

    printf("inbox log appended: %s\n", path);
    return 0;
}

static int tg_build_chat_state_line(const tg_bot_update_summary *update,
                                    char *buffer, unsigned long buffer_size)
{
    char date_text[64];
    char content_text[512];
    char sender_text[128];
    unsigned long position;

    if (update == 0 || buffer == 0 || buffer_size == 0 ||
        !update->has_update || !update->has_message) {
        return 1;
    }

    if (tg_format_inbox_date(update, date_text, sizeof(date_text)) != 0 ||
        tg_inbox_content_text(update, content_text, sizeof(content_text)) != 0) {
        return 1;
    }
    if (update->has_sender_name) {
        strcpy(sender_text, update->sender_name);
    } else {
        strcpy(sender_text, "unknown");
    }
    tg_sanitize_chat_field(sender_text);
    tg_sanitize_chat_field(date_text);
    tg_sanitize_chat_field(content_text);

    position = 0;
    buffer[0] = '\0';
    if (tg_app_append_text(buffer, buffer_size, &position, update->chat_id) != 0 ||
        tg_app_append_char(buffer, buffer_size, &position, '|') != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, sender_text) != 0 ||
        tg_app_append_char(buffer, buffer_size, &position, '|') != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, date_text) != 0 ||
        tg_app_append_char(buffer, buffer_size, &position, '|') != 0 ||
        tg_app_append_text(buffer, buffer_size, &position, content_text) != 0 ||
        tg_app_append_char(buffer, buffer_size, &position, '\n') != 0) {
        return 1;
    }

    return 0;
}

static int tg_chat_state_line_matches(const char *line,
                                      unsigned long line_length,
                                      const char *chat_id)
{
    unsigned long chat_id_length;

    if (line == 0 || chat_id == 0) {
        return 0;
    }
    chat_id_length = (unsigned long)strlen(chat_id);
    return line_length > chat_id_length &&
           strncmp(line, chat_id, chat_id_length) == 0 &&
           line[chat_id_length] == '|';
}

static int tg_update_chat_state_file(const char *path,
                                     const tg_bot_update_summary *update)
{
    tg_file_status file_status;
    char current[16384];
    char rewritten[16384];
    char new_line[1024];
    unsigned long current_length;
    unsigned long input_pos;
    unsigned long output_pos;

    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    if (update == 0 || !update->has_message) {
        return 0;
    }
    if (tg_build_chat_state_line(update, new_line, sizeof(new_line)) != 0) {
        puts("chat state: line too long");
        return 2;
    }

    current[0] = '\0';
    current_length = 0;
    file_status = tg_file_read_text(path, current, sizeof(current),
                                    &current_length);
    if (file_status != TG_FILE_OK && file_status != TG_FILE_OPEN_FAILED) {
        printf("chat state: read failed: %s\n",
               tg_file_status_name(file_status));
        return 2;
    }

    rewritten[0] = '\0';
    input_pos = 0;
    output_pos = 0;
    if (tg_app_append_text(rewritten, sizeof(rewritten), &output_pos,
                           new_line) != 0) {
        puts("chat state: file too large");
        return 2;
    }
    while (input_pos < current_length) {
        unsigned long line_start;
        unsigned long line_end;
        unsigned long line_length;

        line_start = input_pos;
        line_end = line_start;
        while (line_end < current_length &&
               current[line_end] != '\n' &&
               current[line_end] != '\r') {
            ++line_end;
        }
        line_length = line_end - line_start;

        if (tg_chat_state_line_matches(current + line_start, line_length,
                                       update->chat_id)) {
            /* The newest update is already written first; skip the old row. */
        } else if (line_length > 0) {
            if (tg_app_append_n(rewritten, sizeof(rewritten), &output_pos,
                                current + line_start, line_length) != 0 ||
                tg_app_append_char(rewritten, sizeof(rewritten), &output_pos,
                                   '\n') != 0) {
                puts("chat state: file too large");
                return 2;
            }
        }

        input_pos = line_end;
        while (input_pos < current_length &&
               (current[input_pos] == '\n' || current[input_pos] == '\r')) {
            ++input_pos;
        }
    }

    file_status = tg_file_write_text(path, rewritten, output_pos);
    if (file_status != TG_FILE_OK) {
        printf("chat state: write failed: %s\n",
               tg_file_status_name(file_status));
        return 2;
    }

    printf("chat state updated: %s\n", path);
    return 0;
}

static int tg_copy_chat_field(const char *line, unsigned long line_length,
                              unsigned long *position,
                              char *buffer, unsigned long buffer_size)
{
    unsigned long output_pos;

    if (line == 0 || position == 0 || buffer == 0 || buffer_size == 0) {
        return 1;
    }

    output_pos = 0;
    buffer[0] = '\0';
    while (*position < line_length && line[*position] != '|') {
        if (output_pos + 1 >= buffer_size) {
            return 1;
        }
        buffer[output_pos] = line[*position];
        ++output_pos;
        ++(*position);
    }
    buffer[output_pos] = '\0';
    if (*position < line_length && line[*position] == '|') {
        ++(*position);
    }
    return 0;
}

static int tg_parse_chat_state_line(const char *line, unsigned long line_length,
                                    char *chat_id, unsigned long chat_id_size,
                                    char *sender, unsigned long sender_size,
                                    char *date, unsigned long date_size,
                                    char *text, unsigned long text_size)
{
    unsigned long position;

    position = 0;
    if (tg_copy_chat_field(line, line_length, &position,
                           chat_id, chat_id_size) != 0 ||
        tg_copy_chat_field(line, line_length, &position,
                           sender, sender_size) != 0 ||
        tg_copy_chat_field(line, line_length, &position,
                           date, date_size) != 0 ||
        tg_copy_chat_field(line, line_length, &position,
                           text, text_size) != 0) {
        return 1;
    }
    return chat_id[0] == '\0' ? 1 : 0;
}

static int tg_for_each_chat_line(const char *path,
                                 int (*callback)(unsigned long index,
                                                 const char *line,
                                                 unsigned long line_length,
                                                 void *context),
                                 void *context,
                                 int missing_ok)
{
    tg_file_status file_status;
    char content[16384];
    unsigned long content_length;
    unsigned long input_pos;
    unsigned long index;

    if (path == 0 || path[0] == '\0' || callback == 0) {
        return 2;
    }

    file_status = tg_file_read_text(path, content, sizeof(content),
                                    &content_length);
    if (file_status != TG_FILE_OK) {
        if (missing_ok && file_status == TG_FILE_OPEN_FAILED) {
            puts("telegram chats: none");
            return 0;
        }
        printf("telegram chats: read failed: %s\n",
               tg_file_status_name(file_status));
        return 2;
    }

    input_pos = 0;
    index = 1UL;
    while (input_pos < content_length) {
        unsigned long line_start;
        unsigned long line_end;
        unsigned long line_length;

        line_start = input_pos;
        line_end = line_start;
        while (line_end < content_length &&
               content[line_end] != '\n' &&
               content[line_end] != '\r') {
            ++line_end;
        }
        line_length = line_end - line_start;
        if (line_length > 0) {
            if (callback(index, content + line_start, line_length,
                         context) != 0) {
                return 2;
            }
            ++index;
        }

        input_pos = line_end;
        while (input_pos < content_length &&
               (content[input_pos] == '\n' || content[input_pos] == '\r')) {
            ++input_pos;
        }
    }

    if (index == 1UL) {
        puts("telegram chats: none");
    }
    return 0;
}

static int tg_print_chat_line_callback(unsigned long index,
                                       const char *line,
                                       unsigned long line_length,
                                       void *context)
{
    char chat_id[64];
    char sender[128];
    char date[64];
    char text[512];

    (void)context;
    if (tg_parse_chat_state_line(line, line_length,
                                 chat_id, sizeof(chat_id),
                                 sender, sizeof(sender),
                                 date, sizeof(date),
                                 text, sizeof(text)) != 0) {
        printf("%lu | <invalid chat state line>\n", index);
        return 0;
    }

    printf("%lu | chat %s | %s | %s | last: %s\n",
           index, chat_id, sender, date, text);
    return 0;
}

typedef struct tg_find_chat_context {
    unsigned long wanted_index;
    int found;
    char *chat_id;
    unsigned long chat_id_size;
} tg_find_chat_context;

static int tg_find_chat_line_callback(unsigned long index,
                                      const char *line,
                                      unsigned long line_length,
                                      void *context)
{
    tg_find_chat_context *find_context;
    char chat_id[64];
    char sender[128];
    char date[64];
    char text[512];

    find_context = (tg_find_chat_context *)context;
    if (find_context == 0 || index != find_context->wanted_index) {
        return 0;
    }
    if (tg_parse_chat_state_line(line, line_length,
                                 chat_id, sizeof(chat_id),
                                 sender, sizeof(sender),
                                 date, sizeof(date),
                                 text, sizeof(text)) != 0) {
        puts("telegram chats: selected line is invalid");
        return 1;
    }
    if (strlen(chat_id) + 1 > find_context->chat_id_size) {
        puts("telegram chats: selected chat id too long");
        return 1;
    }
    strcpy(find_context->chat_id, chat_id);
    find_context->found = 1;
    return 0;
}

static int tg_find_chat_id_by_index(const char *path,
                                    unsigned long wanted_index,
                                    char *chat_id,
                                    unsigned long chat_id_size)
{
    tg_find_chat_context context;
    int rc;

    if (wanted_index == 0 || chat_id == 0 || chat_id_size == 0) {
        return 2;
    }

    chat_id[0] = '\0';
    context.wanted_index = wanted_index;
    context.found = 0;
    context.chat_id = chat_id;
    context.chat_id_size = chat_id_size;

    rc = tg_for_each_chat_line(path, tg_find_chat_line_callback, &context, 0);
    if (rc != 0) {
        return rc;
    }
    if (!context.found) {
        printf("telegram chats: index not found: %lu\n", wanted_index);
        return 2;
    }
    return 0;
}

static void tg_print_inbox_update_summary(const tg_bot_update_summary *update)
{
    tg_json_status json_status;
    char decoded_text[512];
    char date_text[64];
    char line[1024];
    unsigned long decoded_length;

    if (!update->has_update) {
        puts("inbox: none");
        return;
    }

    printf("inbox update: %s\n", update->update_id);
    if (!update->has_message) {
        puts("inbox message: none");
        return;
    }

    if (tg_format_inbox_date(update, date_text, sizeof(date_text)) == 0) {
        printf("inbox date: %s\n", date_text);
    }
    printf("inbox chat: %s\n", update->chat_id);
    if (update->has_sender_name) {
        printf("inbox sender: %s\n", update->sender_name);
    } else {
        puts("inbox sender: unknown");
    }
    printf("inbox type: %s\n", tg_bot_message_kind_name(update->message_kind));
    if (update->has_text) {
        json_status = tg_json_string_decode(update->text, update->text_length,
                                            decoded_text, sizeof(decoded_text),
                                            &decoded_length);
        if (json_status == TG_JSON_OK) {
            printf("inbox text: %s\n", decoded_text);
        } else {
            printf("inbox text: <decode failed: %s>\n",
                   tg_json_status_name(json_status));
        }
    } else {
        if (update->message_kind == TG_BOT_MESSAGE_PHOTO) {
            puts("inbox text: <photo>");
        } else if (update->message_kind == TG_BOT_MESSAGE_STICKER) {
            puts("inbox text: <sticker>");
        } else if (update->message_kind == TG_BOT_MESSAGE_DOCUMENT) {
            puts("inbox text: <document>");
        } else {
            puts("inbox text: <non-text or unsupported message>");
        }
    }
    if (tg_build_inbox_line(update, line, sizeof(line)) == 0) {
        printf("inbox line: %s\n", line);
    }
}

static int tg_build_echo_text(const tg_bot_update_summary *update,
                              char *buffer, unsigned long buffer_size)
{
    static const char prefix[] = "Echo: ";
    unsigned long prefix_length;
    unsigned long decoded_length;
    tg_json_status json_status;

    if (buffer == 0 || buffer_size == 0 || update == 0 ||
        !update->has_text || update->text == 0) {
        return 1;
    }

    prefix_length = (unsigned long)strlen(prefix);
    if (prefix_length + 1 > buffer_size) {
        return 1;
    }

    strcpy(buffer, prefix);
    json_status = tg_json_string_decode(update->text, update->text_length,
                                        buffer + prefix_length,
                                        buffer_size - prefix_length,
                                        &decoded_length);
    if (json_status != TG_JSON_OK) {
        return 1;
    }
    return 0;
}

static int tg_is_decimal_text(const char *text)
{
    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return 0;
        }
        ++text;
    }
    return 1;
}

static int tg_parse_decimal_ulong(const char *text, unsigned long *value)
{
    unsigned long result;
    unsigned long digit;

    if (value == 0 || !tg_is_decimal_text(text)) {
        return 1;
    }

    result = 0;
    while (*text != '\0') {
        digit = (unsigned long)(*text - '0');
        if (result > (ULONG_MAX - digit) / 10UL) {
            return 1;
        }
        result = result * 10UL + digit;
        ++text;
    }

    *value = result;
    return 0;
}

static void tg_trim_ascii_space(char *text)
{
    unsigned long start;
    unsigned long end;
    unsigned long length;

    if (text == 0) {
        return;
    }

    start = 0;
    while (text[start] == ' ' || text[start] == '\t' ||
           text[start] == '\r' || text[start] == '\n') {
        ++start;
    }

    end = (unsigned long)strlen(text);
    while (end > start &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' ||
            text[end - 1] == '\r' || text[end - 1] == '\n')) {
        --end;
    }

    length = end - start;
    if (start > 0 && length > 0) {
        memmove(text, text + start, length);
    }
    text[length] = '\0';
}

static int tg_build_data_file_path(const char *data_dir, const char *file_name,
                                   char *buffer, unsigned long buffer_size)
{
    unsigned long data_dir_length;
    unsigned long file_name_length;
    unsigned long separator_length;
    char last_char;

    if (file_name == 0 || file_name[0] == '\0' ||
        buffer == 0 || buffer_size == 0) {
        return 1;
    }

    if (data_dir == 0) {
        data_dir = "";
    }

    data_dir_length = (unsigned long)strlen(data_dir);
    file_name_length = (unsigned long)strlen(file_name);
    separator_length = 0;

    if (data_dir_length > 0) {
        last_char = data_dir[data_dir_length - 1];
        if (last_char != ':' && last_char != '/' && last_char != '\\') {
            separator_length = 1;
        }
    }

    if (data_dir_length + separator_length + file_name_length + 1 > buffer_size) {
        return 1;
    }

    if (data_dir_length > 0) {
        memcpy(buffer, data_dir, data_dir_length);
    }
    if (separator_length > 0) {
        buffer[data_dir_length] = '/';
    }
    memcpy(buffer + data_dir_length + separator_length,
           file_name, file_name_length);
    buffer[data_dir_length + separator_length + file_name_length] = '\0';
    return 0;
}

static const char *tg_default_token_file_path(const tg_config *config,
                                             char *buffer,
                                             unsigned long buffer_size)
{
    if (config->token_file_path_override != 0 &&
        config->token_file_path_override[0] != '\0') {
        return config->token_file_path_override;
    }

    if (tg_build_data_file_path(config->data_dir, tg_default_token_file_name,
                                buffer, buffer_size) != 0) {
        puts("telegram token file: default path too long");
        return 0;
    }

    return buffer;
}

static const char *tg_default_named_file_path(const tg_config *config,
                                             const char *file_name,
                                             const char *label,
                                             char *buffer,
                                             unsigned long buffer_size)
{
    if (tg_build_data_file_path(config->data_dir, file_name, buffer,
                                buffer_size) != 0) {
        printf("telegram %s file: default path too long\n", label);
        return 0;
    }

    return buffer;
}

static int tg_load_offset_file(const char *path, char *offset,
                               unsigned long offset_size)
{
    tg_file_status file_status;
    unsigned long offset_length;

    if (offset == 0 || offset_size == 0) {
        return 1;
    }
    offset[0] = '\0';

    file_status = tg_file_read_text(path, offset, offset_size, &offset_length);
    if (file_status == TG_FILE_OPEN_FAILED) {
        return 0;
    }
    if (file_status != TG_FILE_OK) {
        printf("telegram offset file: failed: %s\n",
               tg_file_status_name(file_status));
        return 1;
    }

    tg_trim_ascii_space(offset);
    if (offset[0] != '\0' && !tg_is_decimal_text(offset)) {
        puts("telegram offset file: invalid offset");
        return 1;
    }

    return 0;
}

static int tg_save_offset_file(const char *path, const char *offset)
{
    tg_file_status file_status;
    char line[40];
    unsigned long offset_length;

    if (path == 0 || offset == 0 || !tg_is_decimal_text(offset)) {
        return 1;
    }

    offset_length = (unsigned long)strlen(offset);
    if (offset_length + 2 > sizeof(line)) {
        return 1;
    }

    strcpy(line, offset);
    line[offset_length] = '\n';
    line[offset_length + 1] = '\0';

    file_status = tg_file_write_text(path, line, offset_length + 1);
    if (file_status != TG_FILE_OK) {
        printf("telegram offset file: write failed: %s\n",
               tg_file_status_name(file_status));
        return 1;
    }

    printf("telegram offset saved: %s\n", offset);
    return 0;
}

static int tg_run_telegram_json_test_text(const char *json)
{
    tg_telegram_status telegram_status;
    tg_telegram_response response;
    tg_json_status json_status;

    telegram_status = tg_telegram_parse_response(json, (unsigned long)strlen(json),
                                                 &response, &json_status);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram json test: failed: %s", tg_telegram_status_name(telegram_status));
        if (telegram_status == TG_TELEGRAM_JSON_ERROR ||
            telegram_status == TG_TELEGRAM_MISSING_OK) {
            printf(" / %s", tg_json_status_name(json_status));
        }
        printf("\n");
        return 2;
    }

    tg_print_telegram_response(&response);
    return 0;
}

static int tg_run_telegram_json_test(const tg_config *config)
{
    return tg_run_telegram_json_test_text(config->telegram_json_test_input);
}

static int tg_run_telegram_json_self_test(void)
{
    static const char ok_sample[] = "{\"ok\":true,\"result\":{\"id\":123,\"is_bot\":true}}";
    static const char error_sample[] = "{\"ok\":false,\"description\":\"Unauthorized\"}";

    puts("telegram json self-test: ok sample");
    if (tg_run_telegram_json_test_text(ok_sample) != 0) {
        return 2;
    }

    puts("telegram json self-test: error sample");
    if (tg_run_telegram_json_test_text(error_sample) != 0) {
        return 2;
    }

    return 0;
}

static int tg_run_telegram_path_test(const tg_config *config)
{
    tg_telegram_status telegram_status;
    char path[256];
    unsigned long path_length;

    telegram_status = tg_telegram_build_bot_path(config->telegram_path_test_token,
                                                 config->telegram_path_test_method,
                                                 path, sizeof(path), &path_length);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram path test: failed: %s\n",
               tg_telegram_status_name(telegram_status));
        return 2;
    }

    printf("telegram host: %s\n", tg_telegram_api_host());
    printf("telegram path: %s\n", path);
    printf("telegram path length: %lu\n", path_length);
    return 0;
}

static int tg_run_telegram_http_test_text(const char *http_response)
{
    tg_telegram_status telegram_status;
    tg_telegram_http_response response;
    tg_http_parse_status http_parse_status;
    tg_json_status json_status;

    telegram_status = tg_telegram_parse_http_response(http_response,
                                                      (unsigned long)strlen(http_response),
                                                      &response, &http_parse_status,
                                                      &json_status);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram http test: failed: %s", tg_telegram_status_name(telegram_status));
        if (telegram_status == TG_TELEGRAM_HTTP_PARSE_ERROR) {
            printf(" / %s", tg_http_parse_status_name(http_parse_status));
        } else if (telegram_status == TG_TELEGRAM_JSON_ERROR ||
                   telegram_status == TG_TELEGRAM_MISSING_OK) {
            printf(" / %s", tg_json_status_name(json_status));
        }
        printf("\n");
        return 2;
    }

    printf("telegram http status: %d\n", response.http_status_code);
    tg_print_telegram_response(&response.api);
    return 0;
}

static int tg_run_telegram_http_self_test(void)
{
    static const char ok_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":{\"id\":123,\"is_bot\":true}}";
    static const char error_response[] =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":false,\"description\":\"Unauthorized\"}";

    puts("telegram http self-test: ok response");
    if (tg_run_telegram_http_test_text(ok_response) != 0) {
        return 2;
    }

    puts("telegram http self-test: error response");
    if (tg_run_telegram_http_test_text(error_response) != 0) {
        return 2;
    }

    return 0;
}

static int tg_run_telegram_token_file_path_test(const tg_config *config)
{
    tg_telegram_status telegram_status;
    tg_file_status file_status;
    char token[128];
    char path[256];
    unsigned long token_length;
    unsigned long path_length;

    telegram_status = tg_telegram_load_token_file(config->telegram_token_file_path,
                                                  token, sizeof(token),
                                                  &token_length, &file_status);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram token file test: failed: %s", tg_telegram_status_name(telegram_status));
        if (telegram_status == TG_TELEGRAM_FILE_ERROR) {
            printf(" / %s", tg_file_status_name(file_status));
        }
        printf("\n");
        return 2;
    }

    telegram_status = tg_telegram_build_bot_path(token,
                                                 config->telegram_token_file_method,
                                                 path, sizeof(path), &path_length);
    if (telegram_status != TG_TELEGRAM_OK) {
        printf("telegram token file path test: failed: %s\n",
               tg_telegram_status_name(telegram_status));
        return 2;
    }

    printf("telegram host: %s\n", tg_telegram_api_host());
    printf("telegram method: %s\n", config->telegram_token_file_method);
    printf("telegram token length: %lu\n", token_length);
    printf("telegram path length: %lu\n", path_length);
    printf("telegram path: <redacted>\n");
    return 0;
}

static int tg_run_telegram_default_token_file_path_test(const tg_config *config)
{
    tg_config local_config;
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    local_config = *config;
    local_config.telegram_token_file_path = resolved_path;
    local_config.telegram_token_file_method =
        config->telegram_default_token_file_method;
    return tg_run_telegram_token_file_path_test(&local_config);
}

static int tg_run_telegram_preflight(const tg_config *config)
{
    tg_telegram_status telegram_status;
    tg_file_status file_status;
    tg_https_status https_status;
    tg_tls_status tls_status;
    tg_net_status net_status;
    tg_http_parse_status parse_status;
    tg_http_response parsed_response;
    char token_path[256];
    char token[128];
    char net_error[128];
    char response[4096];
    const char *resolved_path;
    unsigned long token_length;
    unsigned long response_length;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram preflight token file: %s\n", resolved_path);
    telegram_status = tg_telegram_load_token_file(resolved_path, token,
                                                  sizeof(token), &token_length,
                                                  &file_status);
    memset(token, 0, sizeof(token));
    if (telegram_status == TG_TELEGRAM_OK) {
        printf("telegram preflight token file: present, %lu bytes\n",
               token_length);
    } else if (telegram_status == TG_TELEGRAM_FILE_ERROR &&
               file_status == TG_FILE_OPEN_FAILED) {
        puts("telegram preflight token file: missing");
    } else {
        printf("telegram preflight token file: failed: %s",
               tg_telegram_status_name(telegram_status));
        if (telegram_status == TG_TELEGRAM_FILE_ERROR) {
            printf(" / %s", tg_file_status_name(file_status));
        }
        printf("\n");
        return 2;
    }

    net_error[0] = '\0';
    https_status = tg_https_get(tg_telegram_api_host(), "443", "/",
                                response, sizeof(response), &response_length,
                                &tls_status, &net_status, net_error,
                                sizeof(net_error));
    if (https_status != TG_HTTPS_OK) {
        printf("telegram preflight https: failed: %s",
               tg_https_status_name(https_status));
        if (https_status == TG_HTTPS_TLS_ERROR) {
            printf(" / %s", tg_tls_status_name(tls_status));
            if (tls_status == TG_TLS_NET_ERROR) {
                printf(" / %s", tg_net_status_name(net_status));
            }
        }
        if (net_error[0] != '\0') {
            printf(" (%s)", net_error);
        }
        printf("\n");
        return 2;
    }

    printf("telegram preflight https: received %lu bytes\n",
           response_length);
    parse_status = tg_http_parse_response(response, response_length,
                                          &parsed_response);
    if (parse_status != TG_HTTP_PARSE_OK) {
        printf("telegram preflight http parse: failed: %s\n",
               tg_http_parse_status_name(parse_status));
        return 2;
    }

    printf("telegram preflight http status: %d", parsed_response.status_code);
    if (parsed_response.reason_length > 0) {
        printf(" %.*s", (int)parsed_response.reason_length,
               parsed_response.reason);
    }
    printf("\n");

    if (parsed_response.status_code < 200 ||
        parsed_response.status_code > 399) {
        puts("telegram preflight: failed");
        return 2;
    }

    puts("telegram preflight: ok");
    return 0;
}

static int tg_run_telegram_get_me_self_test(void)
{
    static const char get_me_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":{\"id\":123456,\"is_bot\":true,\"first_name\":\"Telegram Amiga\"}}";
    tg_bot_status bot_status;
    tg_bot_call_result result;

    bot_status = tg_bot_parse_get_me_http_response(get_me_response,
                                                   (unsigned long)strlen(get_me_response),
                                                   &result);
    if (bot_status != TG_BOT_OK) {
        printf("telegram getMe self-test: failed: %s", tg_bot_status_name(bot_status));
        if (bot_status == TG_BOT_TELEGRAM_ERROR) {
            printf(" / %s", tg_telegram_status_name(result.telegram_status));
        }
        printf("\n");
        return 2;
    }

    puts("telegram getMe self-test: ok response");
    printf("telegram http status: %d\n", result.response.http_status_code);
    tg_print_telegram_response(&result.response.api);
    return 0;
}

static void tg_print_bot_error(const char *label, tg_bot_status bot_status,
                               const tg_bot_call_result *result,
                               const char *error_buffer)
{
    printf("%s: failed: %s", label, tg_bot_status_name(bot_status));
    if (bot_status == TG_BOT_TOKEN_ERROR || bot_status == TG_BOT_PATH_ERROR ||
        bot_status == TG_BOT_TELEGRAM_ERROR) {
        printf(" / %s", tg_telegram_status_name(result->telegram_status));
    }
    if (bot_status == TG_BOT_TOKEN_ERROR &&
        result->telegram_status == TG_TELEGRAM_FILE_ERROR) {
        printf(" / %s", tg_file_status_name(result->file_status));
    }
    if (bot_status == TG_BOT_HTTPS_ERROR) {
        printf(" / %s", tg_https_status_name(result->https_status));
        if (result->https_status == TG_HTTPS_TLS_ERROR) {
            printf(" / %s", tg_tls_status_name(result->tls_status));
            if (result->tls_status == TG_TLS_NET_ERROR) {
                printf(" / %s", tg_net_status_name(result->net_status));
            }
        }
    }
    if (error_buffer != 0 && error_buffer[0] != '\0') {
        printf(" (%s)", error_buffer);
    }
    printf("\n");
}

static int tg_run_telegram_get_me_path(const char *token_file_path)
{
    tg_bot_status bot_status;
    tg_bot_call_result result;
    char error_buffer[256];
    char http_buffer[8192];
    unsigned long http_response_length;

    bot_status = tg_bot_get_me_from_token_file(token_file_path,
                                               http_buffer, sizeof(http_buffer),
                                               &http_response_length, &result,
                                               error_buffer, sizeof(error_buffer));
    if (bot_status != TG_BOT_OK) {
        tg_print_bot_error("telegram getMe", bot_status, &result, error_buffer);
        return 2;
    }

    printf("telegram getMe: received %lu bytes\n", http_response_length);
    printf("telegram http status: %d\n", result.response.http_status_code);
    tg_print_telegram_response(&result.response.api);

    if (result.response.http_status_code < 200 ||
        result.response.http_status_code > 299 ||
        !result.response.api.ok) {
        return 2;
    }

    return 0;
}

static int tg_run_telegram_get_me(const tg_config *config)
{
    return tg_run_telegram_get_me_path(config->telegram_get_me_token_file_path);
}

static int tg_run_telegram_get_me_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_get_me_path(resolved_path);
}

static int tg_run_telegram_get_updates_self_test(void)
{
    static const char updates_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":["
        "{\"update_id\":1000,\"message\":{\"message_id\":1,\"chat\":{\"id\":123,\"type\":\"private\"},\"text\":\"hello\"}},"
        "{\"update_id\":1001,\"message\":{\"message_id\":2,\"chat\":{\"id\":456,\"type\":\"private\"},\"text\":\"second\"}}"
        "]}";
    static const char empty_updates_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":[]}";
    tg_bot_status bot_status;
    tg_bot_call_result result;
    tg_bot_update_summary update;

    bot_status = tg_bot_parse_get_updates_http_response(updates_response,
                                                        (unsigned long)strlen(updates_response),
                                                        &result);
    if (bot_status != TG_BOT_OK) {
        printf("telegram getUpdates self-test: failed: %s",
               tg_bot_status_name(bot_status));
        if (bot_status == TG_BOT_TELEGRAM_ERROR) {
            printf(" / %s", tg_telegram_status_name(result.telegram_status));
        }
        printf("\n");
        return 2;
    }

    puts("telegram getUpdates self-test: ok response");
    printf("telegram http status: %d\n", result.response.http_status_code);
    tg_print_telegram_response(&result.response.api);

    bot_status = tg_bot_get_updates_first(&result, &update);
    if (bot_status != TG_BOT_OK) {
        printf("telegram getUpdates self-test: update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    tg_print_update_summary(&update);

    bot_status = tg_bot_get_updates_at(&result, 1, &update);
    if (bot_status != TG_BOT_OK || !update.has_update ||
        strcmp(update.update_id, "1001") != 0 ||
        strcmp(update.chat_id, "456") != 0) {
        printf("telegram getUpdates self-test: second update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    tg_print_update_summary(&update);

    bot_status = tg_bot_get_updates_at(&result, 2, &update);
    if (bot_status != TG_BOT_OK || update.has_update) {
        printf("telegram getUpdates self-test: end update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }

    bot_status = tg_bot_parse_get_updates_http_response(
        empty_updates_response,
        (unsigned long)strlen(empty_updates_response),
        &result);
    if (bot_status != TG_BOT_OK) {
        printf("telegram getUpdates empty self-test: failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    bot_status = tg_bot_get_updates_first(&result, &update);
    if (bot_status != TG_BOT_OK || update.has_update) {
        printf("telegram getUpdates empty self-test: failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    puts("telegram getUpdates empty self-test: ok");
    return 0;
}

static int tg_print_get_updates_summaries(const tg_bot_call_result *result,
                                          unsigned long max_updates)
{
    tg_bot_status bot_status;
    tg_bot_update_summary update;
    unsigned long index;

    for (index = 0; index < max_updates; ++index) {
        bot_status = tg_bot_get_updates_at(result, index, &update);
        if (bot_status != TG_BOT_OK) {
            printf("telegram getUpdates: update failed: %s\n",
                   tg_bot_status_name(bot_status));
            return 2;
        }
        if (!update.has_update) {
            if (index == 0) {
                puts("telegram update: none");
            }
            return 0;
        }
        printf("telegram update index: %lu\n", index);
        tg_print_update_summary(&update);
    }

    printf("telegram getUpdates: summary limited to %lu updates\n",
           max_updates);
    return 0;
}

static int tg_run_telegram_get_updates_paths(const char *token_file_path,
                                             const char *offset)
{
    tg_bot_status bot_status;
    tg_bot_call_result result;
    char error_buffer[256];
    char http_buffer[16384];
    unsigned long http_response_length;

    bot_status = tg_bot_get_updates_from_token_file_with_offset(
        token_file_path,
        offset,
        http_buffer, sizeof(http_buffer), &http_response_length, &result,
        error_buffer, sizeof(error_buffer));
    if (bot_status != TG_BOT_OK) {
        tg_print_bot_error("telegram getUpdates", bot_status, &result, error_buffer);
        return 2;
    }

    printf("telegram getUpdates: received %lu bytes\n", http_response_length);
    printf("telegram http status: %d\n", result.response.http_status_code);
    tg_print_telegram_response(&result.response.api);

    if (result.response.http_status_code >= 200 &&
        result.response.http_status_code <= 299 &&
        result.response.api.ok) {
        if (tg_print_get_updates_summaries(&result, 5) != 0) {
            return 2;
        }
    }

    if (result.response.http_status_code < 200 ||
        result.response.http_status_code > 299 ||
        !result.response.api.ok) {
        return 2;
    }

    return 0;
}

static int tg_run_telegram_get_updates(const tg_config *config)
{
    return tg_run_telegram_get_updates_paths(
        config->telegram_get_updates_token_file_path,
        config->telegram_get_updates_offset);
}

static int tg_run_telegram_get_updates_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_get_updates_paths(
        resolved_path, config->telegram_get_updates_default_offset);
}

static int tg_run_telegram_read_once_state_self_test(void)
{
    static const char offset_file_path[] = "telegram-read-once-self-test.tmp";
    static const char updates_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":["
        "{\"update_id\":200,\"message\":{\"message_id\":1,\"chat\":{\"id\":123,\"type\":\"private\"},\"text\":\"read \\\"only\\\"\\nLine \\u20ac\"}},"
        "{\"update_id\":201,\"edited_message\":{\"message_id\":2,\"chat\":{\"id\":456,\"type\":\"private\"},\"text\":\"ignored\"}}"
        "]}";
    static const char expected_text[] =
        "read \"only\"\nLine " "\xe2" "\x82" "\xac";
    tg_bot_status bot_status;
    tg_json_status json_status;
    tg_bot_call_result result;
    tg_bot_update_summary update;
    char next_offset[32];
    char saved_offset[32];
    char decoded_text[128];
    unsigned long decoded_length;

    remove(offset_file_path);

    bot_status = tg_bot_parse_get_updates_http_response(
        updates_response, (unsigned long)strlen(updates_response), &result);
    if (bot_status != TG_BOT_OK) {
        printf("telegram read once state self-test: getUpdates failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }

    bot_status = tg_bot_get_updates_at(&result, 0, &update);
    if (bot_status != TG_BOT_OK || !update.has_update ||
        !update.has_message || !update.has_text) {
        printf("telegram read once state self-test: first update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    json_status = tg_json_string_decode(update.text, update.text_length,
                                        decoded_text, sizeof(decoded_text),
                                        &decoded_length);
    if (json_status != TG_JSON_OK || strcmp(decoded_text, expected_text) != 0) {
        printf("telegram read once state self-test: text decode failed: %s\n",
               tg_json_status_name(json_status));
        return 2;
    }
    bot_status = tg_bot_update_next_offset(&update, next_offset,
                                           sizeof(next_offset));
    if (bot_status != TG_BOT_OK || strcmp(next_offset, "201") != 0) {
        printf("telegram read once state self-test: first offset failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    if (tg_save_offset_file(offset_file_path, next_offset) != 0 ||
        tg_load_offset_file(offset_file_path, saved_offset,
                            sizeof(saved_offset)) != 0 ||
        strcmp(saved_offset, "201") != 0) {
        puts("telegram read once state self-test: first offset file failed");
        remove(offset_file_path);
        return 2;
    }

    bot_status = tg_bot_get_updates_at(&result, 1, &update);
    if (bot_status != TG_BOT_OK || !update.has_update ||
        update.has_message || update.has_text) {
        printf("telegram read once state self-test: second update failed: %s\n",
               tg_bot_status_name(bot_status));
        remove(offset_file_path);
        return 2;
    }
    bot_status = tg_bot_update_next_offset(&update, next_offset,
                                           sizeof(next_offset));
    if (bot_status != TG_BOT_OK || strcmp(next_offset, "202") != 0) {
        printf("telegram read once state self-test: second offset failed: %s\n",
               tg_bot_status_name(bot_status));
        remove(offset_file_path);
        return 2;
    }
    if (tg_save_offset_file(offset_file_path, next_offset) != 0 ||
        tg_load_offset_file(offset_file_path, saved_offset,
                            sizeof(saved_offset)) != 0 ||
        strcmp(saved_offset, "202") != 0) {
        puts("telegram read once state self-test: second offset file failed");
        remove(offset_file_path);
        return 2;
    }

    bot_status = tg_bot_get_updates_at(&result, 2, &update);
    if (bot_status != TG_BOT_OK || update.has_update) {
        printf("telegram read once state self-test: end update failed: %s\n",
               tg_bot_status_name(bot_status));
        remove(offset_file_path);
        return 2;
    }

    remove(offset_file_path);
    puts("telegram read once state self-test: ok");
    printf("telegram read once state self-test: processed 2 update(s)\n");
    printf("telegram next offset: %s\n", next_offset);
    return 0;
}

static int tg_run_telegram_read_once_state_paths(const char *token_file_path,
                                                 const char *offset_file_path,
                                                 tg_read_output_mode output_mode,
                                                 const char *inbox_log_file_path,
                                                 const char *chat_state_file_path)
{
    tg_bot_status bot_status;
    tg_bot_call_result updates_result;
    tg_bot_update_summary update;
    char offset[32];
    char error_buffer[256];
    char http_buffer[16384];
    char next_offset[32];
    unsigned long http_response_length;
    unsigned long index;
    unsigned long processed_count;

    if (tg_load_offset_file(offset_file_path, offset, sizeof(offset)) != 0) {
        return 2;
    }
    if (output_mode == TG_READ_OUTPUT_INBOX) {
        if (offset[0] != '\0') {
            printf("inbox offset loaded: %s\n", offset);
        } else {
            puts("inbox offset loaded: none");
        }
    } else {
        if (offset[0] != '\0') {
            printf("telegram offset loaded: %s\n", offset);
        } else {
            puts("telegram offset loaded: none");
        }
    }

    bot_status = tg_bot_get_updates_from_token_file_with_offset(
        token_file_path,
        offset[0] != '\0' ? offset : 0,
        http_buffer, sizeof(http_buffer), &http_response_length, &updates_result,
        error_buffer, sizeof(error_buffer));
    if (bot_status != TG_BOT_OK) {
        tg_print_bot_error("telegram read once state getUpdates", bot_status,
                           &updates_result, error_buffer);
        return 2;
    }
    if (updates_result.response.http_status_code < 200 ||
        updates_result.response.http_status_code > 299 ||
        !updates_result.response.api.ok) {
        printf("telegram read once state getUpdates: http status %d\n",
               updates_result.response.http_status_code);
        tg_print_telegram_response(&updates_result.response.api);
        return 2;
    }

    processed_count = 0;
    for (index = 0; index < TG_STATE_MAX_UPDATES; ++index) {
        bot_status = tg_bot_get_updates_at(&updates_result, index, &update);
        if (bot_status != TG_BOT_OK) {
            printf("telegram read once state: update failed: %s\n",
                   tg_bot_status_name(bot_status));
            return 2;
        }
        if (!update.has_update) {
            if (index == 0) {
                if (output_mode == TG_READ_OUTPUT_INBOX) {
                    puts("inbox: no update to process");
                } else {
                    puts("telegram read once state: no update to process");
                }
            } else {
                if (output_mode == TG_READ_OUTPUT_INBOX) {
                    printf("inbox processed: %lu update(s)\n", processed_count);
                } else {
                    printf("telegram read once state: processed %lu update(s)\n",
                           processed_count);
                }
            }
            return 0;
        }

        if (output_mode == TG_READ_OUTPUT_INBOX) {
            printf("inbox item: %lu\n", index);
            tg_print_inbox_update_summary(&update);
            if (tg_append_inbox_log_line(inbox_log_file_path, &update) != 0) {
                return 2;
            }
            if (tg_update_chat_state_file(chat_state_file_path, &update) != 0) {
                return 2;
            }
        } else {
            printf("telegram read once state update index: %lu\n", index);
            tg_print_update_summary(&update);
        }

        bot_status = tg_bot_update_next_offset(&update, next_offset,
                                               sizeof(next_offset));
        if (bot_status != TG_BOT_OK) {
            printf("telegram read once state: next offset failed: %s\n",
                   tg_bot_status_name(bot_status));
            return 2;
        }
        if (output_mode == TG_READ_OUTPUT_INBOX) {
            printf("inbox next offset: %s\n", next_offset);
        } else {
            printf("telegram next offset: %s\n", next_offset);
        }
        if (tg_save_offset_file(offset_file_path, next_offset) != 0) {
            return 2;
        }
        ++processed_count;
    }

    bot_status = tg_bot_get_updates_at(&updates_result, TG_STATE_MAX_UPDATES,
                                       &update);
    if (bot_status != TG_BOT_OK) {
        printf("telegram read once state: update limit check failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    if (update.has_update) {
        if (output_mode == TG_READ_OUTPUT_INBOX) {
            printf("inbox update limit reached: %lu\n", TG_STATE_MAX_UPDATES);
        } else {
            printf("telegram read once state: update limit reached: %lu\n",
                   TG_STATE_MAX_UPDATES);
        }
    }
    if (output_mode == TG_READ_OUTPUT_INBOX) {
        printf("inbox processed: %lu update(s)\n", processed_count);
    } else {
        printf("telegram read once state: processed %lu update(s)\n",
               processed_count);
    }
    return 0;
}

static int tg_run_telegram_read_once_state(const tg_config *config)
{
    return tg_run_telegram_read_once_state_paths(
        config->telegram_read_once_state_token_file_path,
        config->telegram_read_once_state_offset_file_path,
        TG_READ_OUTPUT_STATE,
        0,
        0);
}

static int tg_run_telegram_read_once_state_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_read_once_state_paths(
        resolved_path,
        config->telegram_read_once_state_default_offset_file_path,
        TG_READ_OUTPUT_STATE,
        0,
        0);
}

static int tg_run_telegram_read_loop_paths(const char *token_file_path,
                                           const char *offset_file_path,
                                           const char *poll_seconds_text,
                                           const char *max_iterations_text,
                                           tg_read_output_mode output_mode,
                                           const char *inbox_log_file_path,
                                           const char *chat_state_file_path)
{
    unsigned long poll_seconds;
    unsigned long max_iterations;
    unsigned long iteration;
    int rc;

    if (tg_parse_decimal_ulong(poll_seconds_text, &poll_seconds) != 0) {
        puts("telegram read loop: invalid poll seconds");
        return 2;
    }
    if (tg_parse_decimal_ulong(max_iterations_text, &max_iterations) != 0) {
        puts("telegram read loop: invalid max iterations");
        return 2;
    }
    if (poll_seconds > 3600UL) {
        puts("telegram read loop: poll seconds must be <= 3600");
        return 2;
    }
    if (max_iterations == 0UL || max_iterations > 10000UL) {
        puts("telegram read loop: max iterations must be between 1 and 10000");
        return 2;
    }

    for (iteration = 0; iteration < max_iterations; ++iteration) {
        printf("telegram read loop iteration: %lu/%lu\n",
               iteration + 1UL, max_iterations);
        rc = tg_run_telegram_read_once_state_paths(
            token_file_path,
            offset_file_path,
            output_mode,
            inbox_log_file_path,
            chat_state_file_path);
        if (rc != 0) {
            return rc;
        }
        if (iteration + 1UL < max_iterations && poll_seconds > 0UL) {
            printf("telegram read loop sleep: %lu seconds\n", poll_seconds);
            tg_platform_sleep_seconds(poll_seconds);
        }
    }

    return 0;
}

static int tg_run_telegram_read_loop(const tg_config *config)
{
    return tg_run_telegram_read_loop_paths(
        config->telegram_read_loop_token_file_path,
        config->telegram_read_loop_offset_file_path,
        config->telegram_read_loop_poll_seconds,
        config->telegram_read_loop_max_iterations,
        TG_READ_OUTPUT_STATE,
        0,
        0);
}

static int tg_run_telegram_read_loop_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_read_loop_paths(
        resolved_path,
        config->telegram_read_loop_default_offset_file_path,
        config->telegram_read_loop_default_poll_seconds,
        config->telegram_read_loop_default_max_iterations,
        TG_READ_OUTPUT_STATE,
        0,
        0);
}

static int tg_run_telegram_inbox(const tg_config *config)
{
    return tg_run_telegram_read_once_state_paths(
        config->telegram_inbox_token_file_path,
        config->telegram_inbox_offset_file_path,
        TG_READ_OUTPUT_INBOX,
        config->inbox_log_file_path,
        config->chat_state_file_path);
}

static int tg_run_telegram_inbox_self_test(void)
{
    static const char log_file_path[] = "telegram-inbox-self-test.log";
    static const char chat_state_file_path[] = "telegram-chats-self-test.txt";
    static const char updates_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":["
        "{\"update_id\":300,\"message\":{\"message_id\":1,"
        "\"date\":1710000000,"
        "\"from\":{\"id\":42,\"first_name\":\"Michele\",\"username\":\"kaffeine\"},"
        "\"chat\":{\"id\":123,\"type\":\"private\"},"
        "\"text\":\"Inbox \\\"test\\\"\\nLine\"}},"
        "{\"update_id\":301,\"message\":{\"message_id\":2,"
        "\"date\":1710000001,"
        "\"from\":{\"id\":42,\"first_name\":\"Michele\",\"username\":\"kaffeine\"},"
        "\"chat\":{\"id\":123,\"type\":\"private\"},"
        "\"photo\":[{\"file_id\":\"abc\",\"width\":100,\"height\":100}]}}"
        "]}";
    static const char expected_text[] = "Inbox \"test\"\nLine";
    tg_bot_status bot_status;
    tg_json_status json_status;
    tg_bot_call_result result;
    tg_bot_update_summary update;
    tg_bot_update_summary photo_update;
    char next_offset[32];
    char decoded_text[128];
    char log_text[1024];
    char chat_state_text[1024];
    unsigned long decoded_length;
    unsigned long log_length;
    unsigned long chat_state_length;

    remove(log_file_path);
    remove(chat_state_file_path);

    bot_status = tg_bot_parse_get_updates_http_response(
        updates_response, (unsigned long)strlen(updates_response), &result);
    if (bot_status != TG_BOT_OK) {
        printf("telegram inbox self-test: getUpdates failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    bot_status = tg_bot_get_updates_at(&result, 0, &update);
    if (bot_status != TG_BOT_OK || !update.has_update ||
        !update.has_message || !update.has_sender_name ||
        strcmp(update.sender_name, "kaffeine") != 0 || !update.has_text ||
        !update.has_date || strcmp(update.date, "1710000000") != 0 ||
        update.message_kind != TG_BOT_MESSAGE_TEXT) {
        printf("telegram inbox self-test: update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    json_status = tg_json_string_decode(update.text, update.text_length,
                                        decoded_text, sizeof(decoded_text),
                                        &decoded_length);
    if (json_status != TG_JSON_OK || strcmp(decoded_text, expected_text) != 0) {
        printf("telegram inbox self-test: text decode failed: %s\n",
               tg_json_status_name(json_status));
        return 2;
    }
    bot_status = tg_bot_update_next_offset(&update, next_offset,
                                           sizeof(next_offset));
    if (bot_status != TG_BOT_OK || strcmp(next_offset, "301") != 0) {
        printf("telegram inbox self-test: offset failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }

    bot_status = tg_bot_get_updates_at(&result, 1, &photo_update);
    if (bot_status != TG_BOT_OK || !photo_update.has_update ||
        !photo_update.has_message || photo_update.has_text ||
        photo_update.message_kind != TG_BOT_MESSAGE_PHOTO ||
        !photo_update.has_date || strcmp(photo_update.date, "1710000001") != 0) {
        printf("telegram inbox self-test: photo update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }

    tg_print_inbox_update_summary(&update);
    printf("inbox next offset: %s\n", next_offset);
    tg_print_inbox_update_summary(&photo_update);
    if (tg_append_inbox_log_line(log_file_path, &update) != 0 ||
        tg_append_inbox_log_line(log_file_path, &photo_update) != 0 ||
        tg_update_chat_state_file(chat_state_file_path, &update) != 0 ||
        tg_update_chat_state_file(chat_state_file_path, &photo_update) != 0 ||
        tg_file_read_text(log_file_path, log_text, sizeof(log_text),
                          &log_length) != TG_FILE_OK ||
        strstr(log_text, " | text | Inbox \"test\" Line") == 0 ||
        strstr(log_text, " | photo | <photo>") == 0 ||
        tg_file_read_text(chat_state_file_path, chat_state_text,
                          sizeof(chat_state_text),
                          &chat_state_length) != TG_FILE_OK ||
        strstr(chat_state_text, "123|kaffeine|2024-03-09 16:00:01 UTC|<photo>\n") == 0) {
        puts("telegram inbox self-test: local state failed");
        remove(log_file_path);
        remove(chat_state_file_path);
        return 2;
    }
    remove(log_file_path);
    remove(chat_state_file_path);
    puts("telegram inbox self-test: ok");
    return 0;
}

static int tg_run_telegram_inbox_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_read_once_state_paths(
        resolved_path,
        config->telegram_inbox_default_offset_file_path,
        TG_READ_OUTPUT_INBOX,
        config->inbox_log_file_path,
        config->chat_state_file_path);
}

static int tg_run_telegram_inbox_loop(const tg_config *config)
{
    return tg_run_telegram_read_loop_paths(
        config->telegram_inbox_loop_token_file_path,
        config->telegram_inbox_loop_offset_file_path,
        config->telegram_inbox_loop_poll_seconds,
        config->telegram_inbox_loop_max_iterations,
        TG_READ_OUTPUT_INBOX,
        config->inbox_log_file_path,
        config->chat_state_file_path);
}

static int tg_run_telegram_inbox_loop_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_read_loop_paths(
        resolved_path,
        config->telegram_inbox_loop_default_offset_file_path,
        config->telegram_inbox_loop_default_poll_seconds,
        config->telegram_inbox_loop_default_max_iterations,
        TG_READ_OUTPUT_INBOX,
        config->inbox_log_file_path,
        config->chat_state_file_path);
}

static int tg_run_telegram_session_paths(const char *token_file_path,
                                         const char *offset_file_path,
                                         const char *inbox_log_file_path,
                                         const char *chat_state_file_path)
{
    puts("telegram manual session: receive only");
    printf("telegram inbox log file: %s\n", inbox_log_file_path);
    printf("telegram chat state file: %s\n", chat_state_file_path);
    return tg_run_telegram_read_once_state_paths(
        token_file_path,
        offset_file_path,
        TG_READ_OUTPUT_INBOX,
        inbox_log_file_path,
        chat_state_file_path);
}

static int tg_run_telegram_session(const tg_config *config)
{
    return tg_run_telegram_session_paths(
        config->telegram_session_token_file_path,
        config->telegram_session_offset_file_path,
        config->telegram_session_inbox_log_file_path,
        config->telegram_session_chat_state_file_path);
}

static int tg_run_telegram_session_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_session_paths(
        resolved_path,
        config->telegram_session_default_offset_file_path,
        config->telegram_session_default_inbox_log_file_path,
        config->telegram_session_default_chat_state_file_path);
}

static int tg_run_telegram_session_loop_paths(const char *token_file_path,
                                              const char *offset_file_path,
                                              const char *inbox_log_file_path,
                                              const char *chat_state_file_path,
                                              const char *poll_seconds_text,
                                              const char *max_iterations_text)
{
    puts("telegram manual session loop: receive only");
    printf("telegram inbox log file: %s\n", inbox_log_file_path);
    printf("telegram chat state file: %s\n", chat_state_file_path);
    return tg_run_telegram_read_loop_paths(
        token_file_path,
        offset_file_path,
        poll_seconds_text,
        max_iterations_text,
        TG_READ_OUTPUT_INBOX,
        inbox_log_file_path,
        chat_state_file_path);
}

static int tg_run_telegram_session_loop(const tg_config *config)
{
    return tg_run_telegram_session_loop_paths(
        config->telegram_session_loop_token_file_path,
        config->telegram_session_loop_offset_file_path,
        config->telegram_session_loop_inbox_log_file_path,
        config->telegram_session_loop_chat_state_file_path,
        config->telegram_session_loop_poll_seconds,
        config->telegram_session_loop_max_iterations);
}

static int tg_run_telegram_session_loop_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_session_loop_paths(
        resolved_path,
        config->telegram_session_loop_default_offset_file_path,
        config->telegram_session_loop_default_inbox_log_file_path,
        config->telegram_session_loop_default_chat_state_file_path,
        config->telegram_session_loop_default_poll_seconds,
        config->telegram_session_loop_default_max_iterations);
}

static int tg_run_telegram_chats(const tg_config *config)
{
    return tg_for_each_chat_line(config->telegram_chats_file_path,
                                 tg_print_chat_line_callback, 0, 0);
}

static int tg_run_telegram_chats_default(const tg_config *config)
{
    char chats_path[256];
    const char *resolved_path;

    resolved_path = tg_default_named_file_path(
        config, tg_default_chat_state_file_name, "chat state",
        chats_path, sizeof(chats_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram chat state file: %s\n", resolved_path);
    return tg_for_each_chat_line(resolved_path, tg_print_chat_line_callback,
                                 0, 1);
}

static int tg_run_telegram_manual_client_paths(const char *token_file_path,
                                               const char *offset_file_path,
                                               const char *inbox_log_file_path,
                                               const char *chat_state_file_path,
                                               const char *poll_seconds_text,
                                               const char *max_iterations_text)
{
    int rc;

    puts("telegram manual client: receive-only preview");
    rc = tg_run_telegram_session_loop_paths(token_file_path,
                                            offset_file_path,
                                            inbox_log_file_path,
                                            chat_state_file_path,
                                            poll_seconds_text,
                                            max_iterations_text);
    if (rc != 0) {
        return rc;
    }

    puts("telegram manual client chats:");
    rc = tg_for_each_chat_line(chat_state_file_path,
                               tg_print_chat_line_callback, 0, 1);
    if (rc != 0) {
        return rc;
    }

    puts("telegram manual client reply commands:");
    puts("  telegram-test --telegram-reply-default 1 \"Hello from Amiga\"");
    puts("  telegram-test --telegram-send-last-default \"Hello from Amiga\"");
    return 0;
}

static int tg_run_telegram_manual_client(const tg_config *config)
{
    return tg_run_telegram_manual_client_paths(
        config->telegram_manual_client_token_file_path,
        config->telegram_manual_client_offset_file_path,
        config->telegram_manual_client_inbox_log_file_path,
        config->telegram_manual_client_chat_state_file_path,
        config->telegram_manual_client_poll_seconds,
        config->telegram_manual_client_max_iterations);
}

static int tg_run_telegram_manual_client_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_manual_client_paths(
        resolved_path,
        config->telegram_manual_client_default_offset_file_path,
        config->telegram_manual_client_default_inbox_log_file_path,
        config->telegram_manual_client_default_chat_state_file_path,
        config->telegram_manual_client_default_poll_seconds,
        config->telegram_manual_client_default_max_iterations);
}

static int tg_resolve_client_default_paths(const tg_config *config,
                                           const char **offset_file_path,
                                           const char **inbox_log_file_path,
                                           const char **chat_state_file_path,
                                           char *offset_buffer,
                                           unsigned long offset_buffer_size,
                                           char *inbox_buffer,
                                           unsigned long inbox_buffer_size,
                                           char *chat_buffer,
                                           unsigned long chat_buffer_size)
{
    *offset_file_path = tg_default_named_file_path(
        config, tg_default_offset_file_name, "offset",
        offset_buffer, offset_buffer_size);
    *inbox_log_file_path = tg_default_named_file_path(
        config, tg_default_inbox_log_file_name, "inbox log",
        inbox_buffer, inbox_buffer_size);
    *chat_state_file_path = tg_default_named_file_path(
        config, tg_default_chat_state_file_name, "chat state",
        chat_buffer, chat_buffer_size);
    if (*offset_file_path == 0 || *inbox_log_file_path == 0 ||
        *chat_state_file_path == 0) {
        return 2;
    }
    return 0;
}

static int tg_run_telegram_client_paths(const tg_config *config,
                                        const char *token_file_path,
                                        const char *poll_seconds_text,
                                        const char *max_iterations_text)
{
    char offset_path[256];
    char inbox_path[256];
    char chats_path[256];
    const char *resolved_offset_path;
    const char *resolved_inbox_path;
    const char *resolved_chats_path;

    if (poll_seconds_text == 0) {
        poll_seconds_text = tg_default_poll_seconds_text;
    }
    if (max_iterations_text == 0) {
        max_iterations_text = tg_default_max_iterations_text;
    }
    if (tg_resolve_client_default_paths(config,
                                        &resolved_offset_path,
                                        &resolved_inbox_path,
                                        &resolved_chats_path,
                                        offset_path, sizeof(offset_path),
                                        inbox_path, sizeof(inbox_path),
                                        chats_path, sizeof(chats_path)) != 0) {
        return 2;
    }

    printf("telegram offset file: %s\n", resolved_offset_path);
    printf("telegram inbox log file: %s\n", resolved_inbox_path);
    printf("telegram chat state file: %s\n", resolved_chats_path);
    return tg_run_telegram_manual_client_paths(
        token_file_path,
        resolved_offset_path,
        resolved_inbox_path,
        resolved_chats_path,
        poll_seconds_text,
        max_iterations_text);
}

static int tg_run_telegram_client(const tg_config *config)
{
    return tg_run_telegram_client_paths(
        config,
        config->telegram_client_token_file_path,
        config->telegram_client_poll_seconds,
        config->telegram_client_max_iterations);
}

static int tg_run_telegram_client_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_client_paths(
        config,
        resolved_path,
        config->telegram_client_default_poll_seconds,
        config->telegram_client_default_max_iterations);
}

static int tg_run_telegram_client_self_test(void)
{
    static const char chat_state_file_path[] = "telegram-client-self-test-chats.txt";
    static const char updates_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":["
        "{\"update_id\":400,\"message\":{\"message_id\":1,"
        "\"date\":1710000000,"
        "\"from\":{\"id\":42,\"username\":\"kaffeine\"},"
        "\"chat\":{\"id\":123,\"type\":\"private\"},"
        "\"text\":\"first\"}},"
        "{\"update_id\":401,\"message\":{\"message_id\":2,"
        "\"date\":1710000001,"
        "\"from\":{\"id\":43,\"username\":\"friend\"},"
        "\"chat\":{\"id\":456,\"type\":\"private\"},"
        "\"text\":\"second\"}},"
        "{\"update_id\":402,\"message\":{\"message_id\":3,"
        "\"date\":1710000002,"
        "\"from\":{\"id\":42,\"username\":\"kaffeine\"},"
        "\"chat\":{\"id\":123,\"type\":\"private\"},"
        "\"text\":\"latest\"}}"
        "]}";
    tg_bot_status bot_status;
    tg_bot_call_result result;
    tg_bot_update_summary update;
    char chat_id[64];
    unsigned long index;

    remove(chat_state_file_path);
    bot_status = tg_bot_parse_get_updates_http_response(
        updates_response, (unsigned long)strlen(updates_response), &result);
    if (bot_status != TG_BOT_OK) {
        printf("telegram client self-test: getUpdates failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }

    for (index = 0; index < 3UL; ++index) {
        bot_status = tg_bot_get_updates_at(&result, index, &update);
        if (bot_status != TG_BOT_OK || !update.has_update ||
            !update.has_message) {
            printf("telegram client self-test: update failed: %s\n",
                   tg_bot_status_name(bot_status));
            remove(chat_state_file_path);
            return 2;
        }
        if (tg_update_chat_state_file(chat_state_file_path, &update) != 0) {
            remove(chat_state_file_path);
            return 2;
        }
    }

    if (tg_find_chat_id_by_index(chat_state_file_path, 1UL,
                                 chat_id, sizeof(chat_id)) != 0 ||
        strcmp(chat_id, "123") != 0 ||
        tg_find_chat_id_by_index(chat_state_file_path, 2UL,
                                 chat_id, sizeof(chat_id)) != 0 ||
        strcmp(chat_id, "456") != 0) {
        puts("telegram client self-test: chat order failed");
        remove(chat_state_file_path);
        return 2;
    }

    puts("telegram client self-test chats:");
    if (tg_for_each_chat_line(chat_state_file_path,
                              tg_print_chat_line_callback, 0, 0) != 0) {
        remove(chat_state_file_path);
        return 2;
    }
    remove(chat_state_file_path);
    puts("telegram client self-test: ok");
    return 0;
}

static int tg_run_telegram_echo_once_self_test(void)
{
    static const char updates_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":["
        "{\"update_id\":999,\"message\":{\"message_id\":1,\"chat\":{\"id\":123,\"type\":\"private\"},\"text\":\"hello \\\"Amiga\\\"\\nPath C:\\\\Temp \\u0041 \\u20ac \\ud83d\\ude80\"}},"
        "{\"update_id\":1000,\"message\":{\"message_id\":2,\"chat\":{\"id\":456,\"type\":\"private\"},\"text\":\"second\"}}"
        "]}";
    static const char expected_echo_text[] =
        "Echo: hello \"Amiga\"\nPath C:\\Temp A "
        "\xe2" "\x82" "\xac" " "
        "\xf0" "\x9f" "\x9a" "\x80";
    tg_bot_status bot_status;
    tg_bot_call_result result;
    tg_bot_update_summary update;
    tg_bot_update_summary end_update;
    char next_offset[32];
    char echo_text[256];
    char body[512];
    unsigned long body_length;

    bot_status = tg_bot_parse_get_updates_http_response(updates_response,
                                                        (unsigned long)strlen(updates_response),
                                                        &result);
    if (bot_status != TG_BOT_OK) {
        printf("telegram echo once self-test: getUpdates failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    bot_status = tg_bot_get_updates_first(&result, &update);
    if (bot_status != TG_BOT_OK || !update.has_update ||
        !update.has_message || !update.has_text) {
        printf("telegram echo once self-test: update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    bot_status = tg_bot_update_next_offset(&update, next_offset, sizeof(next_offset));
    if (bot_status != TG_BOT_OK) {
        printf("telegram echo once self-test: offset failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    if (strcmp(next_offset, "1000") != 0) {
        printf("telegram echo once self-test: offset mismatch: %s\n", next_offset);
        return 2;
    }
    if (tg_build_echo_text(&update, echo_text, sizeof(echo_text)) != 0) {
        puts("telegram echo once self-test: echo text failed");
        return 2;
    }
    if (strcmp(echo_text, expected_echo_text) != 0) {
        puts("telegram echo once self-test: decoded echo text mismatch");
        printf("telegram echo text: %s\n", echo_text);
        return 2;
    }
    bot_status = tg_bot_build_send_message_body(update.chat_id, echo_text,
                                                body, sizeof(body), &body_length);
    if (bot_status != TG_BOT_OK) {
        printf("telegram echo once self-test: body failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }

    bot_status = tg_bot_get_updates_at(&result, 1, &update);
    if (bot_status != TG_BOT_OK || !update.has_update ||
        !update.has_message || !update.has_text) {
        printf("telegram echo once self-test: second update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    bot_status = tg_bot_update_next_offset(&update, next_offset, sizeof(next_offset));
    if (bot_status != TG_BOT_OK || strcmp(next_offset, "1001") != 0) {
        printf("telegram echo once self-test: second offset failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    if (tg_build_echo_text(&update, echo_text, sizeof(echo_text)) != 0 ||
        strcmp(echo_text, "Echo: second") != 0) {
        puts("telegram echo once self-test: second echo text failed");
        return 2;
    }
    bot_status = tg_bot_build_send_message_body(update.chat_id, echo_text,
                                                body, sizeof(body), &body_length);
    if (bot_status != TG_BOT_OK) {
        printf("telegram echo once self-test: second body failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }

    bot_status = tg_bot_get_updates_at(&result, 2, &end_update);
    if (bot_status != TG_BOT_OK || end_update.has_update) {
        printf("telegram echo once self-test: end update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }

    puts("telegram echo once self-test: ok");
    printf("telegram update id: %s\n", update.update_id);
    printf("telegram next offset: %s\n", next_offset);
    printf("telegram echo chat id: %s\n", update.chat_id);
    printf("telegram echo text: %s\n", echo_text);
    printf("telegram echo body bytes: %lu\n", body_length);
    return 0;
}

static int tg_run_telegram_echo_once_paths(const char *token_file_path,
                                           const char *offset)
{
    tg_bot_status bot_status;
    tg_bot_call_result result;
    tg_bot_update_summary update;
    char error_buffer[256];
    char http_buffer[16384];
    char send_buffer[8192];
    char next_offset[32];
    char echo_text[512];
    unsigned long http_response_length;
    unsigned long send_response_length;

    bot_status = tg_bot_get_updates_from_token_file_with_offset(
        token_file_path,
        offset,
        http_buffer, sizeof(http_buffer), &http_response_length, &result,
        error_buffer, sizeof(error_buffer));
    if (bot_status != TG_BOT_OK) {
        tg_print_bot_error("telegram echo once getUpdates", bot_status,
                           &result, error_buffer);
        return 2;
    }
    if (result.response.http_status_code < 200 ||
        result.response.http_status_code > 299 ||
        !result.response.api.ok) {
        printf("telegram echo once getUpdates: http status %d\n",
               result.response.http_status_code);
        tg_print_telegram_response(&result.response.api);
        return 2;
    }

    bot_status = tg_bot_get_updates_first(&result, &update);
    if (bot_status != TG_BOT_OK) {
        printf("telegram echo once: update failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    tg_print_update_summary(&update);
    if (!update.has_update) {
        puts("telegram echo once: no update to process");
        return 0;
    }

    bot_status = tg_bot_update_next_offset(&update, next_offset, sizeof(next_offset));
    if (bot_status != TG_BOT_OK) {
        printf("telegram echo once: next offset failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    printf("telegram next offset: %s\n", next_offset);

    if (!update.has_message || !update.has_text) {
        puts("telegram echo once: update has no text message");
        return 0;
    }
    if (tg_build_echo_text(&update, echo_text, sizeof(echo_text)) != 0) {
        puts("telegram echo once: echo text too long");
        return 2;
    }

    bot_status = tg_bot_send_message_from_token_file(
        token_file_path,
        update.chat_id, echo_text,
        send_buffer, sizeof(send_buffer), &send_response_length, &result,
        error_buffer, sizeof(error_buffer));
    if (bot_status != TG_BOT_OK) {
        tg_print_bot_error("telegram echo once sendMessage", bot_status,
                           &result, error_buffer);
        return 2;
    }

    printf("telegram echo once sendMessage: received %lu bytes\n",
           send_response_length);
    printf("telegram http status: %d\n", result.response.http_status_code);
    tg_print_telegram_response(&result.response.api);
    if (result.response.http_status_code < 200 ||
        result.response.http_status_code > 299 ||
        !result.response.api.ok) {
        return 2;
    }

    return 0;
}

static int tg_run_telegram_echo_once(const tg_config *config)
{
    return tg_run_telegram_echo_once_paths(
        config->telegram_echo_once_token_file_path,
        config->telegram_echo_once_offset);
}

static int tg_run_telegram_echo_once_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_echo_once_paths(
        resolved_path, config->telegram_echo_once_default_offset);
}

static int tg_run_telegram_echo_once_state_paths(const char *token_file_path,
                                                 const char *offset_file_path)
{
    tg_bot_status bot_status;
    tg_bot_call_result updates_result;
    tg_bot_call_result send_result;
    tg_bot_update_summary update;
    char offset[32];
    char error_buffer[256];
    char http_buffer[16384];
    char send_buffer[8192];
    char next_offset[32];
    char echo_text[512];
    unsigned long http_response_length;
    unsigned long send_response_length;
    unsigned long index;
    unsigned long processed_count;
    unsigned long sent_count;

    if (tg_load_offset_file(offset_file_path, offset, sizeof(offset)) != 0) {
        return 2;
    }
    if (offset[0] != '\0') {
        printf("telegram offset loaded: %s\n", offset);
    } else {
        puts("telegram offset loaded: none");
    }

    bot_status = tg_bot_get_updates_from_token_file_with_offset(
        token_file_path,
        offset[0] != '\0' ? offset : 0,
        http_buffer, sizeof(http_buffer), &http_response_length, &updates_result,
        error_buffer, sizeof(error_buffer));
    if (bot_status != TG_BOT_OK) {
        tg_print_bot_error("telegram echo once state getUpdates", bot_status,
                           &updates_result, error_buffer);
        return 2;
    }
    if (updates_result.response.http_status_code < 200 ||
        updates_result.response.http_status_code > 299 ||
        !updates_result.response.api.ok) {
        printf("telegram echo once state getUpdates: http status %d\n",
               updates_result.response.http_status_code);
        tg_print_telegram_response(&updates_result.response.api);
        return 2;
    }

    processed_count = 0;
    sent_count = 0;
    for (index = 0; index < TG_STATE_MAX_UPDATES; ++index) {
        bot_status = tg_bot_get_updates_at(&updates_result, index, &update);
        if (bot_status != TG_BOT_OK) {
            printf("telegram echo once state: update failed: %s\n",
                   tg_bot_status_name(bot_status));
            return 2;
        }
        if (!update.has_update) {
            if (index == 0) {
                puts("telegram echo once state: no update to process");
            } else {
                printf("telegram echo once state: processed %lu update(s), sent %lu message(s)\n",
                       processed_count, sent_count);
            }
            return 0;
        }

        printf("telegram echo once state update index: %lu\n", index);
        tg_print_update_summary(&update);

        bot_status = tg_bot_update_next_offset(&update, next_offset,
                                               sizeof(next_offset));
        if (bot_status != TG_BOT_OK) {
            printf("telegram echo once state: next offset failed: %s\n",
                   tg_bot_status_name(bot_status));
            return 2;
        }
        printf("telegram next offset: %s\n", next_offset);

        if (!update.has_message || !update.has_text) {
            puts("telegram echo once state: update has no text message");
            if (tg_save_offset_file(offset_file_path, next_offset) != 0) {
                return 2;
            }
            ++processed_count;
            continue;
        }
        if (tg_build_echo_text(&update, echo_text, sizeof(echo_text)) != 0) {
            puts("telegram echo once state: echo text too long");
            return 2;
        }

        bot_status = tg_bot_send_message_from_token_file(
            token_file_path,
            update.chat_id, echo_text,
            send_buffer, sizeof(send_buffer), &send_response_length, &send_result,
            error_buffer, sizeof(error_buffer));
        if (bot_status != TG_BOT_OK) {
            tg_print_bot_error("telegram echo once state sendMessage",
                               bot_status, &send_result, error_buffer);
            return 2;
        }

        printf("telegram echo once state sendMessage: received %lu bytes\n",
               send_response_length);
        printf("telegram http status: %d\n", send_result.response.http_status_code);
        tg_print_telegram_response(&send_result.response.api);
        if (send_result.response.http_status_code < 200 ||
            send_result.response.http_status_code > 299 ||
            !send_result.response.api.ok) {
            return 2;
        }

        if (tg_save_offset_file(offset_file_path, next_offset) != 0) {
            return 2;
        }
        ++processed_count;
        ++sent_count;
    }

    bot_status = tg_bot_get_updates_at(&updates_result, TG_STATE_MAX_UPDATES,
                                       &update);
    if (bot_status != TG_BOT_OK) {
        printf("telegram echo once state: update limit check failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    if (update.has_update) {
        printf("telegram echo once state: update limit reached: %lu\n",
               TG_STATE_MAX_UPDATES);
    }
    printf("telegram echo once state: processed %lu update(s), sent %lu message(s)\n",
           processed_count, sent_count);
    return 0;
}

static int tg_run_telegram_echo_once_state(const tg_config *config)
{
    return tg_run_telegram_echo_once_state_paths(
        config->telegram_echo_once_state_token_file_path,
        config->telegram_echo_once_state_offset_file_path);
}

static int tg_run_telegram_echo_once_state_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_echo_once_state_paths(
        resolved_path,
        config->telegram_echo_once_state_default_offset_file_path);
}

static int tg_run_telegram_echo_loop_paths(const char *token_file_path,
                                           const char *offset_file_path,
                                           const char *poll_seconds_text,
                                           const char *max_iterations_text)
{
    unsigned long poll_seconds;
    unsigned long max_iterations;
    unsigned long iteration;
    int rc;

    if (tg_parse_decimal_ulong(poll_seconds_text, &poll_seconds) != 0) {
        puts("telegram echo loop: invalid poll seconds");
        return 2;
    }
    if (tg_parse_decimal_ulong(max_iterations_text, &max_iterations) != 0) {
        puts("telegram echo loop: invalid max iterations");
        return 2;
    }
    if (poll_seconds > 3600UL) {
        puts("telegram echo loop: poll seconds must be <= 3600");
        return 2;
    }
    if (max_iterations == 0UL || max_iterations > 10000UL) {
        puts("telegram echo loop: max iterations must be between 1 and 10000");
        return 2;
    }

    for (iteration = 0; iteration < max_iterations; ++iteration) {
        printf("telegram echo loop iteration: %lu/%lu\n",
               iteration + 1UL, max_iterations);
        rc = tg_run_telegram_echo_once_state_paths(
            token_file_path,
            offset_file_path);
        if (rc != 0) {
            return rc;
        }
        if (iteration + 1UL < max_iterations && poll_seconds > 0UL) {
            printf("telegram echo loop sleep: %lu seconds\n", poll_seconds);
            tg_platform_sleep_seconds(poll_seconds);
        }
    }

    return 0;
}

static int tg_run_telegram_echo_loop(const tg_config *config)
{
    return tg_run_telegram_echo_loop_paths(
        config->telegram_echo_loop_token_file_path,
        config->telegram_echo_loop_offset_file_path,
        config->telegram_echo_loop_poll_seconds,
        config->telegram_echo_loop_max_iterations);
}

static int tg_run_telegram_echo_loop_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_echo_loop_paths(
        resolved_path,
        config->telegram_echo_loop_default_offset_file_path,
        config->telegram_echo_loop_default_poll_seconds,
        config->telegram_echo_loop_default_max_iterations);
}

static int tg_run_telegram_send_message_self_test(void)
{
    static const char send_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"ok\":true,\"result\":{\"message_id\":42,\"chat\":{\"id\":123},\"text\":\"Hello Amiga\"}}";
    static const char text[] = "Hello \"Amiga\"\nBackslash \\";
    tg_bot_status bot_status;
    tg_bot_call_result result;
    char body[256];
    unsigned long body_length;

    bot_status = tg_bot_build_send_message_body("123", text, body, sizeof(body),
                                                &body_length);
    if (bot_status != TG_BOT_OK) {
        printf("telegram sendMessage self-test: body failed: %s\n",
               tg_bot_status_name(bot_status));
        return 2;
    }
    if (strstr(body, "\"chat_id\":\"123\"") == 0 ||
        strstr(body, "Hello \\\"Amiga\\\"\\nBackslash \\\\") == 0) {
        puts("telegram sendMessage self-test: body mismatch");
        printf("telegram sendMessage body: %s\n", body);
        return 2;
    }

    bot_status = tg_bot_parse_send_message_http_response(send_response,
                                                         (unsigned long)strlen(send_response),
                                                         &result);
    if (bot_status != TG_BOT_OK) {
        printf("telegram sendMessage self-test: failed: %s",
               tg_bot_status_name(bot_status));
        if (bot_status == TG_BOT_TELEGRAM_ERROR) {
            printf(" / %s", tg_telegram_status_name(result.telegram_status));
        }
        printf("\n");
        return 2;
    }

    printf("telegram sendMessage self-test: ok body, %lu bytes\n", body_length);
    printf("telegram http status: %d\n", result.response.http_status_code);
    tg_print_telegram_response(&result.response.api);
    return 0;
}

static int tg_run_telegram_send_message_path(const char *token_file_path,
                                             const char *chat_id,
                                             const char *text)
{
    tg_bot_status bot_status;
    tg_bot_call_result result;
    char error_buffer[256];
    char http_buffer[8192];
    unsigned long http_response_length;

    bot_status = tg_bot_send_message_from_token_file(
        token_file_path,
        chat_id,
        text,
        http_buffer, sizeof(http_buffer), &http_response_length, &result,
        error_buffer, sizeof(error_buffer));
    if (bot_status != TG_BOT_OK) {
        tg_print_bot_error("telegram sendMessage", bot_status, &result, error_buffer);
        return 2;
    }

    printf("telegram sendMessage: received %lu bytes\n", http_response_length);
    printf("telegram http status: %d\n", result.response.http_status_code);
    tg_print_telegram_response(&result.response.api);

    if (result.response.http_status_code < 200 ||
        result.response.http_status_code > 299 ||
        !result.response.api.ok) {
        return 2;
    }

    return 0;
}

static int tg_run_telegram_send_message(const tg_config *config)
{
    return tg_run_telegram_send_message_path(
        config->telegram_send_message_token_file_path,
        config->telegram_send_message_chat_id,
        config->telegram_send_message_text);
}

static int tg_run_telegram_send_message_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_send_message_path(
        resolved_path,
        config->telegram_send_message_default_chat_id,
        config->telegram_send_message_default_text);
}

static int tg_run_telegram_send_chat_path(const char *token_file_path,
                                          const char *chats_file_path,
                                          const char *index_text,
                                          const char *text)
{
    unsigned long index;
    char chat_id[64];

    if (tg_parse_decimal_ulong(index_text, &index) != 0 || index == 0UL) {
        puts("telegram send chat: invalid chat index");
        return 2;
    }
    if (tg_find_chat_id_by_index(chats_file_path, index,
                                 chat_id, sizeof(chat_id)) != 0) {
        return 2;
    }

    printf("telegram send chat index: %lu\n", index);
    printf("telegram send chat id: %s\n", chat_id);
    return tg_run_telegram_send_message_path(token_file_path, chat_id, text);
}

static int tg_run_telegram_send_chat(const tg_config *config)
{
    return tg_run_telegram_send_chat_path(
        config->telegram_send_chat_token_file_path,
        config->telegram_send_chat_file_path,
        config->telegram_send_chat_index,
        config->telegram_send_chat_text);
}

static int tg_run_telegram_send_chat_default(const tg_config *config)
{
    char token_path[256];
    const char *resolved_path;

    resolved_path = tg_default_token_file_path(config, token_path,
                                              sizeof(token_path));
    if (resolved_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_path);
    return tg_run_telegram_send_chat_path(
        resolved_path,
        config->telegram_send_chat_default_file_path,
        config->telegram_send_chat_default_index,
        config->telegram_send_chat_default_text);
}

static int tg_run_telegram_reply_default(const tg_config *config)
{
    char token_path[256];
    char chats_path[256];
    const char *resolved_token_path;
    const char *resolved_chats_path;

    resolved_token_path = tg_default_token_file_path(config, token_path,
                                                    sizeof(token_path));
    resolved_chats_path = tg_default_named_file_path(
        config, tg_default_chat_state_file_name, "chat state",
        chats_path, sizeof(chats_path));
    if (resolved_token_path == 0 || resolved_chats_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_token_path);
    printf("telegram chat state file: %s\n", resolved_chats_path);
    return tg_run_telegram_send_chat_path(
        resolved_token_path,
        resolved_chats_path,
        config->telegram_reply_default_index,
        config->telegram_reply_default_text);
}

static int tg_run_telegram_send_last_default(const tg_config *config)
{
    char token_path[256];
    char chats_path[256];
    const char *resolved_token_path;
    const char *resolved_chats_path;

    resolved_token_path = tg_default_token_file_path(config, token_path,
                                                    sizeof(token_path));
    resolved_chats_path = tg_default_named_file_path(
        config, tg_default_chat_state_file_name, "chat state",
        chats_path, sizeof(chats_path));
    if (resolved_token_path == 0 || resolved_chats_path == 0) {
        return 2;
    }

    printf("telegram token file: %s\n", resolved_token_path);
    printf("telegram chat state file: %s\n", resolved_chats_path);
    return tg_run_telegram_send_chat_path(
        resolved_token_path,
        resolved_chats_path,
        "1",
        config->telegram_send_last_default_text);
}

int tg_app_run(int argc, char **argv)
{
    tg_config config;
    tg_net_status net_status;
    char net_error[128];
    const char *program_name;

    program_name = "telegram-test";
    if (argc > 0 && argv[0] != 0) {
        program_name = argv[0];
    }

    tg_config_init(&config);
    if (tg_config_parse(&config, argc, argv) != 0) {
        tg_config_print_usage(stderr, program_name);
        return 1;
    }

    if (config.show_help) {
        tg_config_print_usage(stdout, program_name);
        return 0;
    }

    tg_log_set_level(config.log_level);

    puts("telegram-amiga bootstrap");
    printf("platform: %s\n", tg_platform_name());
    tg_log(TG_LOG_INFO, "core initialized");
    printf("data dir: %s\n", config.data_dir);

    if (config.run_net_test) {
        net_error[0] = '\0';
        net_status = tg_net_tcp_probe(config.net_test_host, config.net_test_port,
                                      net_error, sizeof(net_error));
        if (net_status != TG_NET_OK) {
            printf("net test: %s:%s failed: %s",
                   config.net_test_host, config.net_test_port,
                   tg_net_status_name(net_status));
            if (net_error[0] != '\0') {
                printf(" (%s)", net_error);
            }
            printf("\n");
            return 2;
        }
        printf("net test: %s:%s ok\n", config.net_test_host, config.net_test_port);
    }

    if (config.run_http_test) {
        return tg_run_http_test(&config);
    }

    if (config.run_http_post_self_test) {
        return tg_run_http_post_self_test();
    }

    if (config.run_https_test) {
        return tg_run_https_test(&config);
    }

    if (config.run_json_test) {
        return tg_run_json_test(&config);
    }

    if (config.run_telegram_json_test) {
        return tg_run_telegram_json_test(&config);
    }

    if (config.run_telegram_json_self_test) {
        return tg_run_telegram_json_self_test();
    }

    if (config.run_telegram_path_test) {
        return tg_run_telegram_path_test(&config);
    }

    if (config.run_telegram_http_self_test) {
        return tg_run_telegram_http_self_test();
    }

    if (config.run_telegram_token_file_path_test) {
        return tg_run_telegram_token_file_path_test(&config);
    }

    if (config.run_telegram_default_token_file_path_test) {
        return tg_run_telegram_default_token_file_path_test(&config);
    }

    if (config.run_telegram_preflight) {
        return tg_run_telegram_preflight(&config);
    }

    if (config.run_telegram_get_me_self_test) {
        return tg_run_telegram_get_me_self_test();
    }

    if (config.run_telegram_get_me) {
        return tg_run_telegram_get_me(&config);
    }

    if (config.run_telegram_get_me_default) {
        return tg_run_telegram_get_me_default(&config);
    }

    if (config.run_telegram_get_updates_self_test) {
        return tg_run_telegram_get_updates_self_test();
    }

    if (config.run_telegram_get_updates) {
        return tg_run_telegram_get_updates(&config);
    }

    if (config.run_telegram_get_updates_default) {
        return tg_run_telegram_get_updates_default(&config);
    }

    if (config.run_telegram_read_once_state_self_test) {
        return tg_run_telegram_read_once_state_self_test();
    }

    if (config.run_telegram_read_once_state) {
        return tg_run_telegram_read_once_state(&config);
    }

    if (config.run_telegram_read_once_state_default) {
        return tg_run_telegram_read_once_state_default(&config);
    }

    if (config.run_telegram_read_loop) {
        return tg_run_telegram_read_loop(&config);
    }

    if (config.run_telegram_read_loop_default) {
        return tg_run_telegram_read_loop_default(&config);
    }

    if (config.run_telegram_inbox_self_test) {
        return tg_run_telegram_inbox_self_test();
    }

    if (config.run_telegram_inbox) {
        return tg_run_telegram_inbox(&config);
    }

    if (config.run_telegram_inbox_default) {
        return tg_run_telegram_inbox_default(&config);
    }

    if (config.run_telegram_inbox_loop) {
        return tg_run_telegram_inbox_loop(&config);
    }

    if (config.run_telegram_inbox_loop_default) {
        return tg_run_telegram_inbox_loop_default(&config);
    }

    if (config.run_telegram_session) {
        return tg_run_telegram_session(&config);
    }

    if (config.run_telegram_session_default) {
        return tg_run_telegram_session_default(&config);
    }

    if (config.run_telegram_session_loop) {
        return tg_run_telegram_session_loop(&config);
    }

    if (config.run_telegram_session_loop_default) {
        return tg_run_telegram_session_loop_default(&config);
    }

    if (config.run_telegram_manual_client) {
        return tg_run_telegram_manual_client(&config);
    }

    if (config.run_telegram_manual_client_default) {
        return tg_run_telegram_manual_client_default(&config);
    }

    if (config.run_telegram_client_self_test) {
        return tg_run_telegram_client_self_test();
    }

    if (config.run_telegram_client) {
        return tg_run_telegram_client(&config);
    }

    if (config.run_telegram_client_default) {
        return tg_run_telegram_client_default(&config);
    }

    if (config.run_telegram_chats) {
        return tg_run_telegram_chats(&config);
    }

    if (config.run_telegram_chats_default) {
        return tg_run_telegram_chats_default(&config);
    }

    if (config.run_telegram_echo_once_self_test) {
        return tg_run_telegram_echo_once_self_test();
    }

    if (config.run_telegram_echo_once) {
        return tg_run_telegram_echo_once(&config);
    }

    if (config.run_telegram_echo_once_default) {
        return tg_run_telegram_echo_once_default(&config);
    }

    if (config.run_telegram_echo_once_state) {
        return tg_run_telegram_echo_once_state(&config);
    }

    if (config.run_telegram_echo_once_state_default) {
        return tg_run_telegram_echo_once_state_default(&config);
    }

    if (config.run_telegram_echo_loop) {
        return tg_run_telegram_echo_loop(&config);
    }

    if (config.run_telegram_echo_loop_default) {
        return tg_run_telegram_echo_loop_default(&config);
    }

    if (config.run_telegram_send_message_self_test) {
        return tg_run_telegram_send_message_self_test();
    }

    if (config.run_telegram_send_message) {
        return tg_run_telegram_send_message(&config);
    }

    if (config.run_telegram_send_message_default) {
        return tg_run_telegram_send_message_default(&config);
    }

    if (config.run_telegram_send_chat) {
        return tg_run_telegram_send_chat(&config);
    }

    if (config.run_telegram_send_chat_default) {
        return tg_run_telegram_send_chat_default(&config);
    }

    if (config.run_telegram_reply_default) {
        return tg_run_telegram_reply_default(&config);
    }

    if (config.run_telegram_send_last_default) {
        return tg_run_telegram_send_last_default(&config);
    }

    return 0;
}
