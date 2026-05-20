#!/bin/sh
set -eu

if [ "$#" -lt 6 ] || [ "$#" -gt 7 ]; then
    echo "Usage: $0 <host> <port> <api-file> <auth-file> <dc-id> <peer-cache-file> [program]" >&2
    exit 1
fi

HOST=$1
PORT=$2
API_FILE=$3
AUTH_FILE=$4
DC_ID=$5
PEER_CACHE_FILE=$6
PROGRAM=${7:-./build/telegram-test}

exec "$PROGRAM" --mtproto-chat-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID" "$PEER_CACHE_FILE"
