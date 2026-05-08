/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#ifndef TG_JSON_H
#define TG_JSON_H

typedef enum tg_json_status {
    TG_JSON_OK = 0,
    TG_JSON_INVALID_ARGUMENT = 1,
    TG_JSON_INVALID_JSON = 2,
    TG_JSON_NOT_FOUND = 3,
    TG_JSON_TYPE_MISMATCH = 4,
    TG_JSON_BUFFER_TOO_SMALL = 5
} tg_json_status;

typedef enum tg_json_value_type {
    TG_JSON_VALUE_NULL = 0,
    TG_JSON_VALUE_BOOL = 1,
    TG_JSON_VALUE_NUMBER = 2,
    TG_JSON_VALUE_STRING = 3,
    TG_JSON_VALUE_OBJECT = 4,
    TG_JSON_VALUE_ARRAY = 5
} tg_json_value_type;

typedef struct tg_json_value {
    tg_json_value_type type;
    /* start/length point into the caller-owned JSON buffer; no allocation is done. */
    const char *start;
    unsigned long length;
    int bool_value;
} tg_json_value;

tg_json_status tg_json_object_get(const char *json, unsigned long json_length,
                                  const char *field_name, tg_json_value *value);
tg_json_status tg_json_object_get_bool(const char *json, unsigned long json_length,
                                       const char *field_name, int *bool_value);
/* Copies the raw JSON string content. Escape decoding will be added when needed. */
tg_json_status tg_json_object_get_string_copy(const char *json, unsigned long json_length,
                                              const char *field_name, char *buffer,
                                              unsigned long buffer_size,
                                              unsigned long *string_length);
const char *tg_json_status_name(tg_json_status status);
const char *tg_json_value_type_name(tg_json_value_type type);

#endif
