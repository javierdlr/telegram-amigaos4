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
cp "$ROOT_DIR/docs/USER_RUNBOOK.md" "$DEST_DIR/USER_RUNBOOK.md"
cp "$ROOT_DIR/docs/HOW_TO_TEST.md" "$DEST_DIR/HOW_TO_TEST.md"
cp "$ROOT_DIR/docs/TLS_CERTIFICATES.md" "$DEST_DIR/TLS_CERTIFICATES.md"
cp "$ROOT_DIR/docs/MTPROTO_QUICK_TEST.md" "$DEST_DIR/MTPROTO_QUICK_TEST.md"
cp "$ROOT_DIR/docs/MTPROTO_REAL_LOGIN.md" "$DEST_DIR/MTPROTO_REAL_LOGIN.md"
mkdir -p "$DEST_DIR/scripts"
cp "$ROOT_DIR"/scripts/mtproto-*.sh "$DEST_DIR/scripts/"
cp "$ROOT_DIR"/scripts/RunMTProto* "$DEST_DIR/"
cp "$ROOT_DIR/scripts/TelegramAmiga" "$DEST_DIR/TelegramAmiga"
if [ -f "$ROOT_DIR/assets/TelegramAmigaOS4.info" ]; then
  cp "$ROOT_DIR/assets/TelegramAmigaOS4.info" "$DEST_DIR/TelegramAmiga.info"
else
  cp "$ROOT_DIR/assets/TelegramAmiga.info" "$DEST_DIR/TelegramAmiga.info"
fi
cp "$ROOT_DIR/scripts/BuildAmigaOS4Offline" "$DEST_DIR/BuildAmigaOS4Offline"
cp "$ROOT_DIR/scripts/BuildAmigaOS4AmiSSL" "$DEST_DIR/BuildAmigaOS4AmiSSL"
cp "$ROOT_DIR/scripts/RunAmigaOS4Preflight" "$DEST_DIR/RunAmigaOS4Preflight"
cp "$ROOT_DIR/scripts/RunAmigaOS4GetMe" "$DEST_DIR/RunAmigaOS4GetMe"
cp "$ROOT_DIR/scripts/RunAmigaOS4HumanChat" "$DEST_DIR/RunAmigaOS4HumanChat"

cat > "$DEST_DIR/README.txt" <<EOF
Telegram Amiga - AmigaOS 4.x pre-alpha tester
Build: $COMMIT_ID

This is a pre-alpha manual text client build for
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
  telegram-test --telegram-text-client-self-test
  telegram-test --mtproto-self-test-fast
  telegram-test --telegram-tls-status

Preflight check, without sending a token:

  Execute RunAmigaOS4Preflight
  telegram-test --telegram-preflight

Live read-only test, after creating telegram-token.txt in the same drawer:

  Execute RunAmigaOS4GetMe
  Execute RunAmigaOS4HumanChat
  telegram-test --telegram-getme-default
  telegram-test --telegram-read-loop-default telegram-offset.txt 5 10
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
Inside chat mode, type normal text to send. Successful chat sends are quiet. Use /watch <seconds> in the top-level prompt or
chat mode to auto-read while waiting, or /watch off to disable it.

For the terse human chat mode, run Execute RunAmigaOS4HumanChat, or run
telegram-test --telegram-human-chat directly. Type normal text to send, press
Enter on an empty line to check for replies, and type quit to exit. If no chat
is selected yet, send a Telegram message to the bot and press Enter, or type
the Bot API chat id once. This mode does not redraw a prompt, waits silently
when there are no updates, keeps log lines out of the chat transcript, and
still appends telegram-inbox.log.

MTProto account login and text chat are also included. This path logs in with a
normal Telegram account and can send real messages to selected peers. It is
pre-alpha; use a test account when possible.

Create telegram-api.txt next to telegram-test:

  <api_id>
  <api_hash>

Keep telegram-api.txt, telegram-auth.bin, phone-code-hash.txt,
telegram-password.txt and telegram-peers.txt private. Do not publish screenshots
or logs showing phone numbers, login codes, 2FA passwords, contact names or
message text.

Start MTProto account chat:

  Double-click TelegramAmiga from Workbench, Ambient or Wanderer.

Shell fallback:

  Execute TelegramAmiga
  Execute RunMTProtoStart

If no saved login exists, this starts the phone/code login wizard first.
After login it uses the DC stored in telegram-auth.bin and enters chat. The peer list can include users, basic groups and channels/supergroups; sending to channels depends on account permissions.

Manual validation and debug commands:

  Execute RunMTProtoLoginWizard

  Execute RunMTProtoCheckLocal
  Execute RunMTProtoInspectAuth
  Execute RunMTProtoLoginSmoke
  Execute RunMTProtoListPeers

Manual chat entry:

  Execute RunMTProtoChat

Pick a peer index and type normal text to send. User peers, basic groups and channels/supergroups use the same text mode when cached peer data is available. Incoming peer messages are
auto-read every 2 seconds while waiting for input. Use /read to poll
immediately, /watch <seconds> to change the interval, /watch off to disable
auto-read, /peer to choose another peer, /peers to refresh the peer cache and
/quit to exit.

If a command reports auth-dc-mismatch, run Execute RunMTProtoInspectAuth and
use the saved dc_id with the matching Telegram endpoint. The latest live
AmigaOS 3.x validation used this explicit form after auth migrated to DC 4:

  Execute RunMTProtoChat 149.154.167.91 443 4 telegram-api.txt telegram-auth.bin telegram-peers.txt telegram-test

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

Quick user instructions are in USER_RUNBOOK.md. Full platform notes are in
README.md. The common checklist is in HOW_TO_TEST.md. TLS validation details
are in TLS_CERTIFICATES.md. MTProto user-login notes are in
MTPROTO_QUICK_TEST.md and MTPROTO_REAL_LOGIN.md.
EOF

if command -v zip >/dev/null 2>&1; then
    (cd "$PACKAGE_ROOT" && rm -f "$DRAWER_NAME.zip" && zip -qr "$DRAWER_NAME.zip" "$DRAWER_NAME")
    echo "Created $PACKAGE_ROOT/$DRAWER_NAME.zip"
fi

echo "Created $DEST_DIR"
