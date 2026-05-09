/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_JSON_H
#define TG_JSON_H

/**
 * JSON parser result.
 *
 * INVALID_JSON means the input bytes are malformed or incomplete JSON.
 * TYPE_MISMATCH means the requested field exists but is not of the requested
 * type, for example asking for a bool when the value is a string.
 * BUFFER_TOO_SMALL is only used by copy helpers that write into caller buffers.
 */
typedef enum tg_json_status {
    TG_JSON_OK = 0,
    TG_JSON_INVALID_ARGUMENT = 1,
    TG_JSON_INVALID_JSON = 2,
    TG_JSON_NOT_FOUND = 3,
    TG_JSON_TYPE_MISMATCH = 4,
    TG_JSON_BUFFER_TOO_SMALL = 5
} tg_json_status;

/**
 * JSON value type reported by tg_json_object_get().
 */
typedef enum tg_json_value_type {
    TG_JSON_VALUE_NULL = 0,
    TG_JSON_VALUE_BOOL = 1,
    TG_JSON_VALUE_NUMBER = 2,
    TG_JSON_VALUE_STRING = 3,
    TG_JSON_VALUE_OBJECT = 4,
    TG_JSON_VALUE_ARRAY = 5
} tg_json_value_type;

/**
 * Borrowed view of a JSON value.
 *
 * start/length point into the caller-owned JSON buffer; no allocation is done.
 * For strings, start points to the raw string content without surrounding
 * quotes. Escape sequences are not decoded yet.
 */
typedef struct tg_json_value {
    tg_json_value_type type;
    const char *start;
    unsigned long length;
    int bool_value;
} tg_json_value;

/**
 * Looks up a top-level field in a JSON object.
 *
 * json is caller-owned and must remain valid while value is used. The function
 * does not allocate, copy or modify json. Only top-level object fields are
 * searched; nested object traversal is intentionally left to higher layers.
 */
tg_json_status tg_json_object_get(const char *json, unsigned long json_length,
                                  const char *field_name, tg_json_value *value);

/**
 * Looks up the first element in a JSON array.
 *
 * array_json is caller-owned and must contain a complete JSON array. The
 * returned value is a borrowed view into array_json. TG_JSON_NOT_FOUND means the
 * array is valid but empty.
 */
tg_json_status tg_json_array_first(const char *array_json, unsigned long array_json_length,
                                   tg_json_value *value);

/**
 * Looks up a top-level boolean field.
 *
 * Returns TG_JSON_TYPE_MISMATCH when the field exists but is not true/false.
 */
tg_json_status tg_json_object_get_bool(const char *json, unsigned long json_length,
                                       const char *field_name, int *bool_value);

/**
 * Looks up a top-level string field and copies it into caller-owned buffer.
 *
 * buffer receives a NUL-terminated string. string_length receives the copied
 * byte count excluding the NUL. Returns TG_JSON_BUFFER_TOO_SMALL if buffer_size
 * cannot hold the string plus terminator. Escape decoding will be added when
 * needed; currently the raw JSON string content is copied.
 */
tg_json_status tg_json_object_get_string_copy(const char *json, unsigned long json_length,
                                              const char *field_name, char *buffer,
                                              unsigned long buffer_size,
                                              unsigned long *string_length);

/**
 * Looks up a top-level number field and copies its raw JSON text.
 *
 * No numeric conversion is performed, which avoids platform-specific integer
 * width issues. The caller receives a NUL-terminated decimal/exponent string.
 */
tg_json_status tg_json_object_get_number_copy(const char *json, unsigned long json_length,
                                              const char *field_name, char *buffer,
                                              unsigned long buffer_size,
                                              unsigned long *number_length);

/**
 * Returns a static string for status. The caller must not free it.
 */
const char *tg_json_status_name(tg_json_status status);

/**
 * Returns a static string for type. The caller must not free it.
 */
const char *tg_json_value_type_name(tg_json_value_type type);

#endif
