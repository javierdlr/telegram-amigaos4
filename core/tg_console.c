/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tg_console.h"

void tg_console_trim_ascii_space(char *text)
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

int tg_console_parse_reply_command(char *line, unsigned long command_length,
                                   char *index_buffer,
                                   unsigned long index_buffer_size,
                                   char **text_start)
{
    char *cursor;
    char *index_start;
    unsigned long index_length;

    if (line == 0 || index_buffer == 0 || index_buffer_size == 0 ||
        text_start == 0) {
        return 1;
    }

    cursor = line + command_length;
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    index_start = cursor;
    while (*cursor >= '0' && *cursor <= '9') {
        ++cursor;
    }
    index_length = (unsigned long)(cursor - index_start);
    if (index_length == 0 || index_length + 1 > index_buffer_size) {
        return 1;
    }
    memcpy(index_buffer, index_start, index_length);
    index_buffer[index_length] = '\0';

    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    if (*cursor == '\0') {
        return 1;
    }
    *text_start = cursor;
    return 0;
}

int tg_console_parse_index_command(char *line, unsigned long command_length,
                                   char *index_buffer,
                                   unsigned long index_buffer_size)
{
    char *cursor;
    char *index_start;
    unsigned long index_length;

    if (line == 0 || index_buffer == 0 || index_buffer_size == 0) {
        return 1;
    }

    cursor = line + command_length;
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    index_start = cursor;
    while (*cursor >= '0' && *cursor <= '9') {
        ++cursor;
    }
    index_length = (unsigned long)(cursor - index_start);
    if (index_length == 0 || index_length + 1 > index_buffer_size) {
        return 1;
    }
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    if (*cursor != '\0') {
        return 1;
    }

    memcpy(index_buffer, index_start, index_length);
    index_buffer[index_length] = '\0';
    return 0;
}
