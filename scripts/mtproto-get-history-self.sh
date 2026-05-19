#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Read a non-identifying Saved Messages history summary.

set -eu

if [ "$#" -lt 6 ] || [ "$#" -gt 7 ]; then
    echo "usage: $0 <host> <port> <api-file> <auth-file> <dc-id> <limit> [program]" >&2
    exit 2
fi

HOST=$1
PORT=$2
API_FILE=$3
AUTH_FILE=$4
DC_ID=$5
LIMIT=$6
PROGRAM=${7:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-get-history-self-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID" "$LIMIT"
