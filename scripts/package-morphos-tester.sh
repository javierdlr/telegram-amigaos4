#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Package a MorphOS pre-alpha tester binary that was built on MorphOS and
# copied back to the Mac. The output is written under build/packages, which is
# ignored by git.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PACKAGE_ROOT=${PACKAGE_ROOT:-"$ROOT_DIR/build/packages"}
BINARY=${BINARY:-"$ROOT_DIR/build/morphos/telegram-test"}
TLS_MODE=${TLS_MODE:-0}
DATE_STAMP=$(date +%Y%m%d)
COMMIT_ID=${COMMIT_ID:-$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)}
DRAWER_NAME="telegram-amiga-morphos-prealpha-$DATE_STAMP-$COMMIT_ID"
DEST_DIR="$PACKAGE_ROOT/$DRAWER_NAME"

if [ ! -f "$BINARY" ]; then
    echo "MorphOS binary not found: $BINARY" >&2
    echo "Build it on MorphOS and copy it to build/morphos/telegram-test first." >&2
    exit 1
fi

rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"

cp "$BINARY" "$DEST_DIR/telegram-test"
cp "$ROOT_DIR/docs/MORPHOS_TESTER.md" "$DEST_DIR/README.md"
cp "$ROOT_DIR/docs/HOW_TO_TEST.md" "$DEST_DIR/HOW_TO_TEST.md"
cp "$ROOT_DIR/docs/TLS_CERTIFICATES.md" "$DEST_DIR/TLS_CERTIFICATES.md"

cat > "$DEST_DIR/README.txt" <<EOF
Telegram Amiga - MorphOS pre-alpha tester
Build: $COMMIT_ID
TLS enabled: $TLS_MODE

This is a pre-alpha manual text client tester for MorphOS.

Minimum offline test:

  telegram-test --telegram-json-self-test
  telegram-test --telegram-get-updates-self-test
  telegram-test --telegram-read-once-state-self-test
  telegram-test --telegram-offset-state-self-test
  telegram-test --telegram-inbox-self-test
  telegram-test --telegram-echo-once-self-test
  telegram-test --telegram-send-message-self-test
  telegram-test --telegram-console-self-test
  telegram-test --telegram-client-state-self-test
  telegram-test --telegram-client-self-test
  telegram-test --telegram-text-client-self-test
  telegram-test --telegram-tls-status

If this package was built with TLS enabled, optional live tests are:

  telegram-test --telegram-preflight
  telegram-test --telegram-getme-default
  telegram-test --telegram-read-loop-default telegram-offset.txt 5 10
  telegram-test --telegram-inbox-default telegram-offset.txt
  telegram-test --telegram-inbox-loop-default telegram-offset.txt 5 10
  telegram-test --inbox-log-file telegram-inbox.log --telegram-inbox-loop-default telegram-offset.txt 5 10
  telegram-test --telegram-session-default telegram-offset.txt telegram-inbox.log telegram-chats.txt
  telegram-test --telegram-session-loop-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
  telegram-test --telegram-manual-client-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
  telegram-test --telegram-client-default
  telegram-test --telegram-client-console

Inside the console, use /read or /refresh to poll, /chats to list saved chats,
/last to show the last inbox line, /status to show local state, /open <index>
or a bare numeric index to enter a line-oriented chat, /send <text> to send
to the selected chat, reply <index> <text> for an explicit indexed send and
/quit to quit. The selected chat is persisted in telegram-selected-chat.txt.
Inside chat mode, type normal text to send. Successful chat sends are quiet
and print only me: <text>. Use /watch <seconds> in the top-level prompt or
chat mode to auto-read while waiting, or /watch off to disable it.

  telegram-test --telegram-chats telegram-chats.txt
  telegram-test --telegram-chats-default
  telegram-test --telegram-send-chat-default telegram-chats.txt 1 "Hello from MorphOS"
  telegram-test --telegram-reply-default 1 "Hello from MorphOS"
  telegram-test --telegram-send-last-default "Hello from MorphOS"
  telegram-test --telegram-send-default <chat-id> "Hello from MorphOS"

For Bot API tests, create telegram-token.txt in this drawer. The file must
contain only the bot token. Never publish the token or screenshots that show it.

To create a test bot, open Telegram, talk to @BotFather, send /newbot, choose a
display name and a username ending in bot, then copy the token into
telegram-token.txt. If the token is exposed, revoke it with BotFather /revoke.

Certificate validation is not enabled by default. Use unverified TLS only
with test bots and disposable tokens.

OpenSSL builds can request certificate validation with:

  telegram-test --tls-verify --tls-ca-file ca-bundle.crt --https-test api.telegram.org 443 /
  telegram-test --tls-verify --tls-ca-file ca-bundle.crt --telegram-preflight

Full platform notes are in README.md. The common checklist is in HOW_TO_TEST.md.
TLS validation details are in TLS_CERTIFICATES.md.
EOF

if command -v zip >/dev/null 2>&1; then
    (cd "$PACKAGE_ROOT" && rm -f "$DRAWER_NAME.zip" && zip -qr "$DRAWER_NAME.zip" "$DRAWER_NAME")
    echo "Created $PACKAGE_ROOT/$DRAWER_NAME.zip"
fi

echo "Created $DEST_DIR"
