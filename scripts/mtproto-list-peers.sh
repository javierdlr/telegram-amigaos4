#!/bin/sh
set -eu

if [ "$#" -lt 7 ] || [ "$#" -gt 8 ]; then
    echo "Usage: $0 <host> <port> <api-file> <auth-file> <dc-id> <limit> <peer-cache-file> [program]" >&2
    exit 1
fi

HOST=$1
PORT=$2
API_FILE=$3
AUTH_FILE=$4
DC_ID=$5
LIMIT=$6
PEER_CACHE_FILE=$7
PROGRAM=${8:-./build/telegram-test}

exec "$PROGRAM" --mtproto-auth-list-peers-file \
    "$HOST" "$PORT" "$API_FILE" "$AUTH_FILE" "$DC_ID" "$LIMIT" \
    "$PEER_CACHE_FILE"
