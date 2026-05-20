#!/bin/sh
set -eu

if [ "$#" -lt 8 ] || [ "$#" -gt 9 ]; then
    echo "Usage: $0 <host> <port> <api-file> <auth-file> <dc-id> <peer-cache-file> <peer-index> <text> [program]" >&2
    exit 1
fi

HOST=$1
PORT=$2
API_FILE=$3
AUTH_FILE=$4
DC_ID=$5
PEER_CACHE_FILE=$6
PEER_INDEX=$7
TEXT=$8
PROGRAM=${9:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-send-peer-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID" \
    "$PEER_CACHE_FILE" "$PEER_INDEX" "$TEXT"
