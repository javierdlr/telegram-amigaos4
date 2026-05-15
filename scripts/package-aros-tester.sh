#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Build and package an AROS One i386 alt-abiv0 pre-alpha tester. The output is
# written under build/packages, which is ignored by git.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PACKAGE_ROOT=${PACKAGE_ROOT:-"$ROOT_DIR/build/packages"}
TARGET=${TARGET:-"$ROOT_DIR/build/aros-i386-abiv0/telegram-test"}
ENABLE_TLS=${ENABLE_TLS:-0}
DATE_STAMP=$(date +%Y%m%d)
COMMIT_ID=${COMMIT_ID:-$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)}
DRAWER_NAME="telegram-amiga-aros-i386-abiv0-prealpha-$DATE_STAMP-$COMMIT_ID"
DEST_DIR="$PACKAGE_ROOT/$DRAWER_NAME"

make -C "$ROOT_DIR" -f Makefile.aros-i386-abiv0 \
    clean all \
    ENABLE_TLS="$ENABLE_TLS" \
    TARGET="$TARGET"

rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"

cp "$TARGET" "$DEST_DIR/telegram-test"
cp "$ROOT_DIR/docs/AROS_TESTER.md" "$DEST_DIR/README.md"
cp "$ROOT_DIR/docs/HOW_TO_TEST.md" "$DEST_DIR/HOW_TO_TEST.md"
cp "$ROOT_DIR/docs/TLS_CERTIFICATES.md" "$DEST_DIR/TLS_CERTIFICATES.md"

cat > "$DEST_DIR/README.txt" <<EOF
Telegram Amiga - AROS One i386 alt-abiv0 pre-alpha tester
Build: $COMMIT_ID
TLS enabled: $ENABLE_TLS

This is not a usable Telegram client yet. It is a diagnostic tester for AROS.

This package targets AROS One i386 alt-abiv0. Do not use it as proof that other
AROS ABIs are compatible.

Minimum offline test:

  telegram-test --help
  telegram-test --telegram-json-self-test
  telegram-test --telegram-get-updates-self-test
  telegram-test --telegram-read-once-state-self-test
  telegram-test --telegram-inbox-self-test
  telegram-test --telegram-echo-once-self-test
  telegram-test --telegram-send-message-self-test
  telegram-test --telegram-console-self-test
  telegram-test --telegram-client-state-self-test
  telegram-test --telegram-client-self-test
  telegram-test --telegram-tls-status

Plain TCP/HTTP diagnostics, no Telegram token required:

  telegram-test --net-test example.com 80
  telegram-test --http-test example.com 80 /

If this package was built with TLS enabled, optional supervised live tests are:

  telegram-test --https-test api.telegram.org 443 /
  telegram-test --telegram-preflight
  telegram-test --telegram-getme-default
  telegram-test --telegram-read-loop-default telegram-offset.txt 5 10
  telegram-test --telegram-client-default
  telegram-test --telegram-client-console
  telegram-test --telegram-chats-default
  telegram-test --telegram-reply-default 1 "Hello from AROS"
  telegram-test --telegram-send-last-default "Hello from AROS"

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

If this package was built with TLS disabled, live Telegram commands are expected
to report unsupported.

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
