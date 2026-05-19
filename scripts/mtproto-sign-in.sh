#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Complete MTProto login after auth.sendCode using a local api file.

set -eu

if [ "$#" -lt 8 ] || [ "$#" -gt 9 ]; then
    echo "usage: $0 <host> <port> <api-file> <auth-file> <phone> <code-hash-file> <code> <dc-id> [program]" >&2
    exit 2
fi

HOST=$1
PORT=$2
API_FILE=$3
AUTH_FILE=$4
PHONE=$5
CODE_HASH_FILE=$6
CODE=$7
DC_ID=$8
PROGRAM=${9:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-sign-in-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$PHONE" "$CODE_HASH_FILE" "$CODE" "$DC_ID"
