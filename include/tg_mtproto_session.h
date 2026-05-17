/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 */

#ifndef TG_MTPROTO_SESSION_H
#define TG_MTPROTO_SESSION_H

typedef enum tg_mtproto_session_status {
    TG_MTPROTO_SESSION_OK = 0,
    TG_MTPROTO_SESSION_INVALID_ARGUMENT = 1,
    TG_MTPROTO_SESSION_FILE_ERROR = 2,
    TG_MTPROTO_SESSION_PARSE_ERROR = 3,
    TG_MTPROTO_SESSION_BUFFER_TOO_SMALL = 4
} tg_mtproto_session_status;

typedef struct tg_mtproto_session {
    unsigned long dc_id;
    unsigned long auth_key_id_hi;
    unsigned long auth_key_id_lo;
    unsigned long server_salt_hi;
    unsigned long server_salt_lo;
    unsigned char session_id[8];
} tg_mtproto_session;

void tg_mtproto_session_init(tg_mtproto_session *session);
tg_mtproto_session_status tg_mtproto_session_save(const char *path,
                                                  const tg_mtproto_session *session);
tg_mtproto_session_status tg_mtproto_session_load(const char *path,
                                                  tg_mtproto_session *session);
const char *tg_mtproto_session_status_name(tg_mtproto_session_status status);
int tg_mtproto_session_self_test(void);

#endif
