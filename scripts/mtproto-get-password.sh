#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Probe Telegram 2FA/SRP metadata using api_id from a local ignored file.

set -eu

if [ "$#" -lt 5 ] || [ "$#" -gt 6 ]; then
    echo "usage: $0 <host> <port> <api-file> <auth-file> <dc-id> [program]" >&2
    exit 2
fi

HOST=$1
PORT=$2
API_FILE=$3
AUTH_FILE=$4
DC_ID=$5
PROGRAM=${6:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-get-password-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID"
