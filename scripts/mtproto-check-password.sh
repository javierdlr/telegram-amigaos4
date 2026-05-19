#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Complete MTProto 2FA login using a local ignored password file.

set -eu

if [ "$#" -lt 6 ] || [ "$#" -gt 7 ]; then
    echo "usage: $0 <host> <port> <api-id> <auth-file> <dc-id> <password-file> [program]" >&2
    exit 2
fi

HOST=$1
PORT=$2
API_ID=$3
AUTH_FILE=$4
DC_ID=$5
PASSWORD_FILE=$6
PROGRAM=${7:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-check-password \
    "$HOST" "$PORT" "$API_ID" "$AUTH_FILE" "$DC_ID" "$PASSWORD_FILE"
