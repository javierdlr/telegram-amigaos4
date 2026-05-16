#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Build a local AmigaOS 3.x pre-alpha tester package. The output is written
# under build/packages, which is ignored by git.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TOOLCHAIN_BIN=${TOOLCHAIN_BIN:-"$HOME/amiga-dev/toolchain/bin"}
AMISSL_SDK=${AMISSL_SDK:-"$HOME/amiga-dev/sdk/AmiSSL/Developer"}
AMISSL_API_VERSION=${AMISSL_API_VERSION:-AMISSL_V340}
PACKAGE_ROOT=${PACKAGE_ROOT:-"$ROOT_DIR/build/packages"}
TARGET=${TARGET:-"$ROOT_DIR/build/amigaos3/telegram-test-amissl"}
DATE_STAMP=$(date +%Y%m%d)
COMMIT_ID=${COMMIT_ID:-$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)}
DRAWER_NAME="telegram-amiga-amigaos3-prealpha-$DATE_STAMP-$COMMIT_ID"
DEST_DIR="$PACKAGE_ROOT/$DRAWER_NAME"

if [ ! -x "$TOOLCHAIN_BIN/m68k-amigaos-gcc" ]; then
    echo "m68k-amigaos-gcc not found in TOOLCHAIN_BIN=$TOOLCHAIN_BIN" >&2
    exit 1
fi

if [ ! -d "$AMISSL_SDK/include" ]; then
    echo "AmiSSL SDK include directory not found: $AMISSL_SDK/include" >&2
    exit 1
fi

PATH="$TOOLCHAIN_BIN:$PATH" make -C "$ROOT_DIR" -f Makefile.amigaos3-gcc \
    clean all \
    ENABLE_AMISSL=1 \
    AMISSL_API_VERSION="$AMISSL_API_VERSION" \
    AMISSL_SDK="$AMISSL_SDK" \
    TARGET="$TARGET"

rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"

cp "$TARGET" "$DEST_DIR/telegram-test"
cp "$ROOT_DIR/docs/AMIGAOS3_TESTER.md" "$DEST_DIR/README.md"
cp "$ROOT_DIR/docs/HOW_TO_TEST.md" "$DEST_DIR/HOW_TO_TEST.md"
cp "$ROOT_DIR/docs/TLS_CERTIFICATES.md" "$DEST_DIR/TLS_CERTIFICATES.md"
cp "$ROOT_DIR/scripts/RunAmigaOS3Preflight" "$DEST_DIR/RunAmigaOS3Preflight"
cp "$ROOT_DIR/scripts/RunAmigaOS3GetMe" "$DEST_DIR/RunAmigaOS3GetMe"
cp "$ROOT_DIR/scripts/RunAmigaOS3HumanChat" "$DEST_DIR/RunAmigaOS3HumanChat"

cat > "$DEST_DIR/README.txt" <<EOF
Telegram Amiga - AmigaOS 3.x pre-alpha tester
Build: $COMMIT_ID

This is a pre-alpha manual text client tester for
AmigaOS 3.x with ixemul and AmiSSL v5.

This tester is built for the conservative AmiSSL API $AMISSL_API_VERSION so it
can use a normal system AmiSSL v5 installation. No AmiSSL runtime is bundled.

On 68060 systems, use the AmiSSL 68060 library variant. On Vampire or 68080
systems, use the AmiSSL 68020/030/040/080 library variant.

Minimum supervised test:

  Execute RunAmigaOS3Preflight

Use Execute for the helper script. If you want to run it directly and your
unpacker cleared Amiga protection bits, run:

  Protect RunAmigaOS3Preflight +se

Expected successful HTTPS result:

  telegram preflight http status: 302 Moved Temporarily
  telegram preflight: ok

For Bot API tests, create telegram-token.txt in this drawer. The file must
contain only the bot token. Never publish the token or screenshots that show it.

To create a test bot, open Telegram, talk to @BotFather, send /newbot, choose a
display name and a username ending in bot, then copy the token into
telegram-token.txt. If the token is exposed, revoke it with BotFather /revoke.

Useful commands:

  Execute RunAmigaOS3GetMe
  Execute RunAmigaOS3HumanChat
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
  telegram-test --telegram-getme-default
  telegram-test --telegram-get-updates-default
  telegram-test --telegram-read-once-state-default telegram-offset.txt
  telegram-test --telegram-read-loop-default telegram-offset.txt 5 10
  telegram-test --telegram-inbox-default telegram-offset.txt
  telegram-test --telegram-inbox-loop-default telegram-offset.txt 5 10
  telegram-test --inbox-log-file telegram-inbox.log --telegram-inbox-loop-default telegram-offset.txt 5 10
  telegram-test --telegram-session-default telegram-offset.txt telegram-inbox.log telegram-chats.txt
  telegram-test --telegram-session-loop-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
  telegram-test --telegram-manual-client-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
  telegram-test --telegram-client-default
  telegram-test --telegram-client-console
  telegram-test --telegram-human-chat

Inside the console, use /read or /refresh to poll, /chats to list saved chats,
/last to show the last inbox line, /status to show local state, /open <index>
or a bare numeric index to enter a line-oriented chat, /send <text> to send
to the selected chat, /send-id <chat-id> <text> to send directly when the chat
list is empty, reply <index> <text> for an explicit indexed send and /quit to
quit. The selected chat is persisted in telegram-selected-chat.txt.
Inside chat mode, type normal text to send. Successful chat sends are quiet
and print only me: <text>. Use /watch <seconds> in the top-level prompt or
chat mode to auto-read while waiting, or /watch off to disable it.

For the terse human chat mode, run telegram-test --telegram-human-chat. Type
normal text to send, press Enter on an empty line to check for replies, and
type quit to exit. If no chat is selected yet, send a Telegram message to the
bot and press Enter, or type the Bot API chat id once. This mode keeps log lines out of the chat
transcript, but still appends telegram-inbox.log.

  telegram-test --telegram-chats telegram-chats.txt
  telegram-test --telegram-chats-default
  telegram-test --telegram-send-chat-default telegram-chats.txt 1 "Hello from AmigaOS 3.x"
  telegram-test --telegram-reply-default 1 "Hello from AmigaOS 3.x"
  telegram-test --telegram-send-last-default "Hello from AmigaOS 3.x"
  telegram-test --telegram-send-default <chat-id> "Hello from AmigaOS 3.x"
  telegram-test --telegram-echo-once-state-default telegram-offset.txt
  telegram-test --telegram-echo-loop-default telegram-offset.txt 5 10

AmiSSL certificate validation is opt-in with --tls-verify and either
--tls-ca-file or --tls-ca-path. Keep using test bots and disposable tokens
until this path has more independent target-side testing.

  telegram-test --tls-verify --tls-ca-file ca-bundle.crt --https-test api.telegram.org 443 /
  telegram-test --tls-verify --tls-ca-file ca-bundle.crt --telegram-preflight

Tip: to avoid copying a raw chat id, send a message to the bot, run
telegram-client-default, then use telegram-chats-default and
telegram-reply-default with the numbered chat index. Index 1 is the most
recently updated chat.

Full platform notes are in README.md. The common checklist is in HOW_TO_TEST.md.
TLS validation details are in TLS_CERTIFICATES.md.
EOF

if command -v zip >/dev/null 2>&1; then
    (cd "$PACKAGE_ROOT" && rm -f "$DRAWER_NAME.zip" && zip -qr "$DRAWER_NAME.zip" "$DRAWER_NAME")
    echo "Created $PACKAGE_ROOT/$DRAWER_NAME.zip"
fi

echo "Created $DEST_DIR"
