/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_json.h"

static int tg_json_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int tg_json_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static void tg_json_skip_ws(const char *json, unsigned long json_length, unsigned long *pos)
{
    while (*pos < json_length && tg_json_is_space(json[*pos])) {
        *pos = *pos + 1;
    }
}

/* JSON strings may contain escaped quotes; this scans to the real closing quote. */
static tg_json_status tg_json_scan_string(const char *json, unsigned long json_length,
                                          unsigned long pos, unsigned long *content_start,
                                          unsigned long *content_length,
                                          unsigned long *next_pos)
{
    unsigned long i;

    if (pos >= json_length || json[pos] != '"') {
        return TG_JSON_INVALID_JSON;
    }

    i = pos + 1;
    while (i < json_length) {
        if (json[i] == '\\') {
            if (i + 1 >= json_length) {
                return TG_JSON_INVALID_JSON;
            }
            i += 2;
        } else if (json[i] == '"') {
            *content_start = pos + 1;
            *content_length = i - (pos + 1);
            *next_pos = i + 1;
            return TG_JSON_OK;
        } else {
            ++i;
        }
    }

    return TG_JSON_INVALID_JSON;
}

static int tg_json_key_equals(const char *json, unsigned long key_start,
                              unsigned long key_length, const char *field_name)
{
    /* Telegram field names are plain ASCII, so escaped object keys are not needed yet. */
    return strlen(field_name) == key_length &&
           strncmp(json + key_start, field_name, key_length) == 0;
}

static tg_json_status tg_json_scan_literal(const char *json, unsigned long json_length,
                                           unsigned long pos, const char *literal,
                                           unsigned long *next_pos)
{
    unsigned long literal_length;

    literal_length = (unsigned long)strlen(literal);
    if (pos + literal_length > json_length) {
        return TG_JSON_INVALID_JSON;
    }
    if (strncmp(json + pos, literal, literal_length) != 0) {
        return TG_JSON_INVALID_JSON;
    }

    *next_pos = pos + literal_length;
    return TG_JSON_OK;
}

static tg_json_status tg_json_scan_number(const char *json, unsigned long json_length,
                                          unsigned long pos, unsigned long *next_pos)
{
    unsigned long i;

    i = pos;
    if (i < json_length && json[i] == '-') {
        ++i;
    }
    if (i >= json_length || !tg_json_is_digit(json[i])) {
        return TG_JSON_INVALID_JSON;
    }
    if (json[i] == '0') {
        ++i;
    } else {
        while (i < json_length && tg_json_is_digit(json[i])) {
            ++i;
        }
    }
    if (i < json_length && json[i] == '.') {
        ++i;
        if (i >= json_length || !tg_json_is_digit(json[i])) {
            return TG_JSON_INVALID_JSON;
        }
        while (i < json_length && tg_json_is_digit(json[i])) {
            ++i;
        }
    }
    if (i < json_length && (json[i] == 'e' || json[i] == 'E')) {
        ++i;
        if (i < json_length && (json[i] == '+' || json[i] == '-')) {
            ++i;
        }
        if (i >= json_length || !tg_json_is_digit(json[i])) {
            return TG_JSON_INVALID_JSON;
        }
        while (i < json_length && tg_json_is_digit(json[i])) {
            ++i;
        }
    }

    *next_pos = i;
    return TG_JSON_OK;
}

/* Objects and arrays can be nested; this keeps enough state to skip them safely. */
static tg_json_status tg_json_scan_container(const char *json, unsigned long json_length,
                                             unsigned long pos, char open_char,
                                             char close_char, unsigned long *next_pos)
{
    unsigned long i;
    unsigned long depth;

    if (pos >= json_length || json[pos] != open_char) {
        return TG_JSON_INVALID_JSON;
    }

    i = pos + 1;
    depth = 1;
    while (i < json_length) {
        if (json[i] == '"') {
            unsigned long string_start;
            unsigned long string_length;
            tg_json_status status;

            status = tg_json_scan_string(json, json_length, i, &string_start,
                                         &string_length, &i);
            if (status != TG_JSON_OK) {
                return status;
            }
        } else if (json[i] == open_char) {
            ++depth;
            ++i;
        } else if (json[i] == close_char) {
            --depth;
            ++i;
            if (depth == 0) {
                *next_pos = i;
                return TG_JSON_OK;
            }
        } else {
            ++i;
        }
    }

    return TG_JSON_INVALID_JSON;
}

static tg_json_status tg_json_scan_value(const char *json, unsigned long json_length,
                                         unsigned long pos, tg_json_value *value,
                                         unsigned long *next_pos)
{
    unsigned long value_start;
    unsigned long value_length;
    tg_json_status status;

    tg_json_skip_ws(json, json_length, &pos);
    if (pos >= json_length) {
        return TG_JSON_INVALID_JSON;
    }

    value->start = json + pos;
    value->length = 0;
    value->bool_value = 0;

    if (json[pos] == '"') {
        status = tg_json_scan_string(json, json_length, pos, &value_start,
                                     &value_length, next_pos);
        if (status != TG_JSON_OK) {
            return status;
        }
        value->type = TG_JSON_VALUE_STRING;
        value->start = json + value_start;
        value->length = value_length;
        return TG_JSON_OK;
    }

    if (json[pos] == '{') {
        status = tg_json_scan_container(json, json_length, pos, '{', '}', next_pos);
        if (status != TG_JSON_OK) {
            return status;
        }
        value->type = TG_JSON_VALUE_OBJECT;
        value->length = *next_pos - pos;
        return TG_JSON_OK;
    }

    if (json[pos] == '[') {
        status = tg_json_scan_container(json, json_length, pos, '[', ']', next_pos);
        if (status != TG_JSON_OK) {
            return status;
        }
        value->type = TG_JSON_VALUE_ARRAY;
        value->length = *next_pos - pos;
        return TG_JSON_OK;
    }

    if (json[pos] == 't') {
        status = tg_json_scan_literal(json, json_length, pos, "true", next_pos);
        value->type = TG_JSON_VALUE_BOOL;
        value->length = 4;
        value->bool_value = 1;
        return status;
    }

    if (json[pos] == 'f') {
        status = tg_json_scan_literal(json, json_length, pos, "false", next_pos);
        value->type = TG_JSON_VALUE_BOOL;
        value->length = 5;
        value->bool_value = 0;
        return status;
    }

    if (json[pos] == 'n') {
        status = tg_json_scan_literal(json, json_length, pos, "null", next_pos);
        value->type = TG_JSON_VALUE_NULL;
        value->length = 4;
        return status;
    }

    status = tg_json_scan_number(json, json_length, pos, next_pos);
    if (status == TG_JSON_OK) {
        value->type = TG_JSON_VALUE_NUMBER;
        value->length = *next_pos - pos;
    }
    return status;
}

tg_json_status tg_json_object_get(const char *json, unsigned long json_length,
                                  const char *field_name, tg_json_value *value)
{
    unsigned long pos;

    if (value != 0) {
        value->type = TG_JSON_VALUE_NULL;
        value->start = 0;
        value->length = 0;
        value->bool_value = 0;
    }
    if (json == 0 || field_name == 0 || value == 0) {
        return TG_JSON_INVALID_ARGUMENT;
    }

    pos = 0;
    tg_json_skip_ws(json, json_length, &pos);
    if (pos >= json_length || json[pos] != '{') {
        return TG_JSON_INVALID_JSON;
    }
    ++pos;

    for (;;) {
        unsigned long key_start;
        unsigned long key_length;
        unsigned long next_pos;
        tg_json_status status;

        tg_json_skip_ws(json, json_length, &pos);
        if (pos < json_length && json[pos] == '}') {
            return TG_JSON_NOT_FOUND;
        }

        status = tg_json_scan_string(json, json_length, pos, &key_start,
                                     &key_length, &pos);
        if (status != TG_JSON_OK) {
            return status;
        }

        tg_json_skip_ws(json, json_length, &pos);
        if (pos >= json_length || json[pos] != ':') {
            return TG_JSON_INVALID_JSON;
        }
        ++pos;

        status = tg_json_scan_value(json, json_length, pos, value, &next_pos);
        if (status != TG_JSON_OK) {
            return status;
        }
        if (tg_json_key_equals(json, key_start, key_length, field_name)) {
            return TG_JSON_OK;
        }

        pos = next_pos;
        tg_json_skip_ws(json, json_length, &pos);
        if (pos < json_length && json[pos] == ',') {
            ++pos;
        } else if (pos < json_length && json[pos] == '}') {
            return TG_JSON_NOT_FOUND;
        } else {
            return TG_JSON_INVALID_JSON;
        }
    }
}

tg_json_status tg_json_object_get_bool(const char *json, unsigned long json_length,
                                       const char *field_name, int *bool_value)
{
    tg_json_value value;
    tg_json_status status;

    if (bool_value == 0) {
        return TG_JSON_INVALID_ARGUMENT;
    }

    status = tg_json_object_get(json, json_length, field_name, &value);
    if (status != TG_JSON_OK) {
        return status;
    }
    if (value.type != TG_JSON_VALUE_BOOL) {
        return TG_JSON_TYPE_MISMATCH;
    }

    *bool_value = value.bool_value;
    return TG_JSON_OK;
}

tg_json_status tg_json_object_get_string_copy(const char *json, unsigned long json_length,
                                              const char *field_name, char *buffer,
                                              unsigned long buffer_size,
                                              unsigned long *string_length)
{
    tg_json_value value;
    tg_json_status status;

    if (string_length != 0) {
        *string_length = 0;
    }
    if (buffer == 0 || buffer_size == 0 || string_length == 0) {
        return TG_JSON_INVALID_ARGUMENT;
    }

    status = tg_json_object_get(json, json_length, field_name, &value);
    if (status != TG_JSON_OK) {
        return status;
    }
    if (value.type != TG_JSON_VALUE_STRING) {
        return TG_JSON_TYPE_MISMATCH;
    }
    if (value.length + 1 > buffer_size) {
        return TG_JSON_BUFFER_TOO_SMALL;
    }

    memcpy(buffer, value.start, value.length);
    buffer[value.length] = '\0';
    *string_length = value.length;
    return TG_JSON_OK;
}

const char *tg_json_status_name(tg_json_status status)
{
    switch (status) {
    case TG_JSON_OK:
        return "ok";
    case TG_JSON_INVALID_ARGUMENT:
        return "invalid-argument";
    case TG_JSON_INVALID_JSON:
        return "invalid-json";
    case TG_JSON_NOT_FOUND:
        return "not-found";
    case TG_JSON_TYPE_MISMATCH:
        return "type-mismatch";
    case TG_JSON_BUFFER_TOO_SMALL:
        return "buffer-too-small";
    default:
        return "unknown";
    }
}

const char *tg_json_value_type_name(tg_json_value_type type)
{
    switch (type) {
    case TG_JSON_VALUE_NULL:
        return "null";
    case TG_JSON_VALUE_BOOL:
        return "bool";
    case TG_JSON_VALUE_NUMBER:
        return "number";
    case TG_JSON_VALUE_STRING:
        return "string";
    case TG_JSON_VALUE_OBJECT:
        return "object";
    case TG_JSON_VALUE_ARRAY:
        return "array";
    default:
        return "unknown";
    }
}
