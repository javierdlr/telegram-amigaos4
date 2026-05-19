#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Inspect a saved MTProto auth file without printing auth key material.

set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 <auth-file> [program]" >&2
    exit 2
fi

AUTH_FILE=$1
PROGRAM=${2:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-inspect "$AUTH_FILE"
