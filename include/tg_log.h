/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_LOG_H
#define TG_LOG_H

typedef enum tg_log_level {
    TG_LOG_ERROR = 0,
    TG_LOG_WARN = 1,
    TG_LOG_INFO = 2,
    TG_LOG_DEBUG = 3
} tg_log_level;

void tg_log_set_level(tg_log_level level);
tg_log_level tg_log_get_level(void);
void tg_log(tg_log_level level, const char *message);
const char *tg_log_level_name(tg_log_level level);

#endif
