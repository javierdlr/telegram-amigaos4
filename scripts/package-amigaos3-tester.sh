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
PACKAGE_ROOT=${PACKAGE_ROOT:-"$ROOT_DIR/build/packages"}
TARGET=${TARGET:-"$ROOT_DIR/build/amigaos3/telegram-test-amissl"}
DATE_STAMP=$(date +%Y%m%d)
COMMIT_ID=$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)
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
    AMISSL_SDK="$AMISSL_SDK" \
    TARGET="$TARGET"

rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"

cp "$TARGET" "$DEST_DIR/telegram-test"
cp "$ROOT_DIR/docs/AMIGAOS3_TESTER.md" "$DEST_DIR/README.md"
cp "$ROOT_DIR/scripts/RunAmigaOS3Preflight" "$DEST_DIR/RunAmigaOS3Preflight"

cat > "$DEST_DIR/README.txt" <<EOF
Telegram Amiga - AmigaOS 3.x pre-alpha tester
Build: $COMMIT_ID

This is not a usable Telegram client yet. It is a diagnostic tester for
AmigaOS 3.x with ixemul and AmiSSL v5.

On 68060 systems, use the AmiSSL 68060 library variant. On Vampire or 68080
systems, use the AmiSSL 68020/030/040/080 library variant.

Minimum supervised test:

  Execute RunAmigaOS3Preflight

Expected successful HTTPS result:

  telegram preflight http status: 302 Moved Temporarily
  telegram preflight: ok

For Bot API tests, create telegram-token.txt in this drawer. The file must
contain only the bot token. Never publish the token or screenshots that show it.

Useful commands:

  telegram-test --telegram-json-self-test
  telegram-test --telegram-get-updates-self-test
  telegram-test --telegram-read-once-state-self-test
  telegram-test --telegram-echo-once-self-test
  telegram-test --telegram-send-message-self-test
  telegram-test --telegram-getme-default
  telegram-test --telegram-get-updates-default
  telegram-test --telegram-read-once-state-default telegram-offset.txt
  telegram-test --telegram-send-message-default <chat-id> "Hello from AmigaOS 3.x"
  telegram-test --telegram-echo-once-state-default telegram-offset.txt
  telegram-test --telegram-echo-loop-default telegram-offset.txt 5 10

Certificate validation is not enabled yet. Use this only with test bots and
disposable tokens.

Full notes are in README.md.
EOF

if command -v zip >/dev/null 2>&1; then
    (cd "$PACKAGE_ROOT" && rm -f "$DRAWER_NAME.zip" && zip -qr "$DRAWER_NAME.zip" "$DRAWER_NAME")
    echo "Created $PACKAGE_ROOT/$DRAWER_NAME.zip"
fi

echo "Created $DEST_DIR"
