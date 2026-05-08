/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_FILE_H
#define TG_FILE_H

/**
 * File helper result.
 *
 * TOO_LARGE means the caller-owned destination buffer cannot hold the complete
 * file plus a final NUL terminator. OPEN_FAILED and READ_FAILED are deliberately
 * generic for now to keep the portable API small.
 */
typedef enum tg_file_status {
    TG_FILE_OK = 0,
    TG_FILE_INVALID_ARGUMENT = 1,
    TG_FILE_OPEN_FAILED = 2,
    TG_FILE_READ_FAILED = 3,
    TG_FILE_TOO_LARGE = 4
} tg_file_status;

/**
 * Reads a whole text file into caller-owned buffer.
 *
 * buffer receives a NUL-terminated byte string and text_length receives the byte
 * count excluding the terminator. The function does not allocate memory.
 */
tg_file_status tg_file_read_text(const char *path, char *buffer,
                                 unsigned long buffer_size,
                                 unsigned long *text_length);

/**
 * Returns a static string for status. The caller must not free it.
 */
const char *tg_file_status_name(tg_file_status status);

#endif
