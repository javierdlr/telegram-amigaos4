#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Validate local MTProto login files without printing secrets.

set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 5 ]; then
    echo "usage: $0 <api-file> <auth-file> [password-file] [code-hash-file] [program]" >&2
    exit 2
fi

API_FILE=$1
AUTH_FILE=$2
PASSWORD_FILE=${3:-}
CODE_HASH_FILE=${4:-}
PROGRAM=${5:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-check-local-files \
    "$API_FILE" "$AUTH_FILE" "$PASSWORD_FILE" "$CODE_HASH_FILE"
