#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Start MTProto login using api_id/api_hash from a local ignored file.

set -eu

if [ "$#" -lt 7 ] || [ "$#" -gt 8 ]; then
    echo "usage: $0 <host> <port> <dc-id> <api-file> <phone> <auth-file> <code-hash-file> [program]" >&2
    exit 2
fi

HOST=$1
PORT=$2
DC_ID=$3
API_FILE=$4
PHONE=$5
AUTH_FILE=$6
CODE_HASH_FILE=$7
PROGRAM=${8:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-send-code-file \
    "$HOST" "$PORT" "$DC_ID" "$API_FILE" "$PHONE" "$AUTH_FILE" "$CODE_HASH_FILE"
