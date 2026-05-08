/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "tg_file.h"

tg_file_status tg_file_read_text(const char *path, char *buffer,
                                 unsigned long buffer_size,
                                 unsigned long *text_length)
{
    FILE *file;
    unsigned long used;
    int ch;

    if (text_length != 0) {
        *text_length = 0;
    }
    if (path == 0 || path[0] == '\0' || buffer == 0 ||
        buffer_size == 0 || text_length == 0) {
        return TG_FILE_INVALID_ARGUMENT;
    }

    file = fopen(path, "rb");
    if (file == 0) {
        buffer[0] = '\0';
        return TG_FILE_OPEN_FAILED;
    }

    used = 0;
    for (;;) {
        ch = fgetc(file);
        if (ch == EOF) {
            break;
        }
        if (used + 1 >= buffer_size) {
            fclose(file);
            buffer[0] = '\0';
            return TG_FILE_TOO_LARGE;
        }
        buffer[used] = (char)ch;
        ++used;
    }

    if (ferror(file)) {
        fclose(file);
        buffer[0] = '\0';
        return TG_FILE_READ_FAILED;
    }

    fclose(file);
    buffer[used] = '\0';
    *text_length = used;
    return TG_FILE_OK;
}

const char *tg_file_status_name(tg_file_status status)
{
    switch (status) {
    case TG_FILE_OK:
        return "ok";
    case TG_FILE_INVALID_ARGUMENT:
        return "invalid-argument";
    case TG_FILE_OPEN_FAILED:
        return "open-failed";
    case TG_FILE_READ_FAILED:
        return "read-failed";
    case TG_FILE_TOO_LARGE:
        return "too-large";
    default:
        return "unknown";
    }
}
