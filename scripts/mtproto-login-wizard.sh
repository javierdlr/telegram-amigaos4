#!/bin/sh
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT

if [ "$#" -lt 6 ] || [ "$#" -gt 7 ]; then
    echo "usage: $0 <host> <port> <dc-id> <api-file> <auth-file> <code-hash-file> [program]" >&2
    exit 2
fi

HOST=$1
PORT=$2
DC_ID=$3
API_FILE=$4
AUTH_FILE=$5
CODE_HASH_FILE=$6
PROGRAM=${7:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-login-wizard-file \
    "$HOST" "$PORT" "$DC_ID" "$API_FILE" "$AUTH_FILE" "$CODE_HASH_FILE"
