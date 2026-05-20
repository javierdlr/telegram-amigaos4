#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Serial saved-session MTProto smoke. Do not run multiple copies against the
# same auth file.

set -eu

if [ "$#" -lt 5 ] || [ "$#" -gt 8 ]; then
    echo "usage: $0 <host> <port> <api-file> <auth-file> <dc-id> [limit] [password-file|-] [program]" >&2
    exit 2
fi

HOST=$1
PORT=$2
API_FILE=$3
AUTH_FILE=$4
DC_ID=$5
LIMIT=${6:-10}
if [ "$#" -eq 7 ] && [ -x "$7" ]; then
    PASSWORD_FILE=
    PROGRAM=$7
else
    PASSWORD_FILE=${7:-}
    PROGRAM=${8:-./build/telegram-test}
fi

if [ "$PASSWORD_FILE" = "-" ]; then
    PASSWORD_FILE=
fi

"$PROGRAM" --mtproto-auth-check-local-files \
    "$API_FILE" "$AUTH_FILE" "$PASSWORD_FILE"
"$PROGRAM" --mtproto-auth-inspect "$AUTH_FILE"
"$PROGRAM" --mtproto-auth-status-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID"
"$PROGRAM" --mtproto-auth-get-config-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID"
"$PROGRAM" --mtproto-auth-get-dialogs-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID" "$LIMIT"
"$PROGRAM" --mtproto-auth-get-history-self-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID" "$LIMIT"
