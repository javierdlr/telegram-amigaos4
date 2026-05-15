#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Package an AmigaOS 4.x pre-alpha tester binary that was built separately and
# copied back to the Mac. The output is written under build/packages, which is
# ignored by git.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PACKAGE_ROOT=${PACKAGE_ROOT:-"$ROOT_DIR/build/packages"}
BINARY=${BINARY:-"$ROOT_DIR/build/amigaos4/telegram-test"}
DATE_STAMP=$(date +%Y%m%d)
COMMIT_ID=${COMMIT_ID:-$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)}
DRAWER_NAME="telegram-amiga-amigaos4-prealpha-$DATE_STAMP-$COMMIT_ID"
DEST_DIR="$PACKAGE_ROOT/$DRAWER_NAME"

if [ ! -f "$BINARY" ]; then
    echo "AmigaOS 4.x binary not found: $BINARY" >&2
    echo "Build it with Makefile.amigaos4 or copy it to build/amigaos4/telegram-test first." >&2
    exit 1
fi

rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"

cp "$BINARY" "$DEST_DIR/telegram-test"
cp "$ROOT_DIR/docs/AMIGAOS4_TESTER.md" "$DEST_DIR/README.md"
cp "$ROOT_DIR/docs/HOW_TO_TEST.md" "$DEST_DIR/HOW_TO_TEST.md"
cp "$ROOT_DIR/docs/TLS_CERTIFICATES.md" "$DEST_DIR/TLS_CERTIFICATES.md"
cp "$ROOT_DIR/scripts/BuildAmigaOS4Offline" "$DEST_DIR/BuildAmigaOS4Offline"
cp "$ROOT_DIR/scripts/BuildAmigaOS4AmiSSL" "$DEST_DIR/BuildAmigaOS4AmiSSL"

cat > "$DEST_DIR/README.txt" <<EOF
Telegram Amiga - AmigaOS 4.x pre-alpha tester
Build: $COMMIT_ID

This is not a usable Telegram client yet. It is a pre-alpha technical build for
checking startup, AmiSSL, HTTPS reachability and a small set of Telegram Bot
API commands.

Minimum offline test:

  telegram-test --help
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
  telegram-test --telegram-tls-status

Preflight check, without sending a token:

  telegram-test --telegram-preflight

Live read-only test, after creating telegram-token.txt in the same drawer:

  telegram-test --telegram-getme-default
  telegram-test --telegram-read-loop-default telegram-offset.txt 5 10
  telegram-test --telegram-manual-client-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
  telegram-test --telegram-client-default
  telegram-test --telegram-client-console

Inside the console, use p/poll/read to poll, l/list to list saved chats,
i/last/inbox to show the last inbox line, s/status to show local state,
chat <index>, open <index> or a bare numeric index to enter a line-oriented
chat, r/send/reply <index> <text> to send a controlled reply, h/help for help
and q/quit to quit. Inside chat mode, type normal text to send. Successful
chat sends are quiet and print only me: <text>. Use watch <seconds> in the
top-level console to auto-read while waiting at the prompt, or watch off to
disable it. Chat mode auto-reads every 5 seconds by default; use /watch
<seconds>, /watch off, /read, /poll, /p, /list, /chats, /last, /status, /back
and /quit.

Manual send by saved chat index:

  telegram-test --telegram-chats telegram-chats.txt
  telegram-test --telegram-send-chat-default telegram-chats.txt 1 "Hello from AmigaOS 4.x"
  telegram-test --telegram-reply-default 1 "Hello from AmigaOS 4.x"
  telegram-test --telegram-send-last-default "Hello from AmigaOS 4.x"

If this package also contains source files and gcc is available, the helper
BuildAmigaOS4Offline can build and run the same offline checks from the project
drawer. BuildAmigaOS4AmiSSL builds the AmiSSL-enabled tester when AmiSSL SDK
stubs are installed.

This package does not include Telegram tokens, SDK files or AmiSSL runtime
files.

To create a test bot, open Telegram, talk to @BotFather, send /newbot, choose a
display name and a username ending in bot, then copy the token into
telegram-token.txt. If the token is exposed, revoke it with BotFather /revoke.

AmiSSL certificate validation is opt-in with --tls-verify and either
--tls-ca-file or --tls-ca-path. Keep using test bots and disposable tokens
until this path has more independent target-side testing.

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
