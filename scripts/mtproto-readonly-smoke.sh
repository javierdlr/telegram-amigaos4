#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Run the read-only MTProto post-login smoke sequence.

set -eu

if [ "$#" -lt 5 ] || [ "$#" -gt 7 ]; then
    echo "usage: $0 <host> <port> <api-file> <auth-file> <dc-id> [limit] [program]" >&2
    exit 2
fi

HOST=$1
PORT=$2
API_FILE=$3
AUTH_FILE=$4
DC_ID=$5
LIMIT=${6:-10}
PROGRAM=${7:-./build/telegram-test}

"$PROGRAM" --mtproto-auth-status-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID"
"$PROGRAM" --mtproto-auth-get-config-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID"
"$PROGRAM" --mtproto-auth-get-dialogs-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID" "$LIMIT"
"$PROGRAM" --mtproto-auth-get-history-self-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID" "$LIMIT"
