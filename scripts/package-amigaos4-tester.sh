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
COMMIT_ID=$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)
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
cp "$ROOT_DIR/scripts/BuildAmigaOS4Offline" "$DEST_DIR/BuildAmigaOS4Offline"

cat > "$DEST_DIR/README.txt" <<EOF
Telegram Amiga - AmigaOS 4.x pre-alpha tester
Build: $COMMIT_ID

This is not a usable Telegram client yet. It is a pre-alpha tester for
AmigaOS 4.x. If built with ENABLE_AMISSL=1, it can perform live HTTPS
Telegram Bot API checks.

Minimum offline test:

  telegram-test --help
  telegram-test --telegram-json-self-test
  telegram-test --telegram-get-updates-self-test
  telegram-test --telegram-read-once-state-self-test
  telegram-test --telegram-inbox-self-test
  telegram-test --telegram-echo-once-self-test
  telegram-test --telegram-send-message-self-test

Live read-only test, with a token file already copied to RAM:

  telegram-test --telegram-preflight
  telegram-test --telegram-getme RAM:telegram-token.txt
  telegram-test --telegram-read-loop RAM:telegram-token.txt RAM:telegram-offset-os4.txt 0 1

If this package also contains source files and gcc is available, the helper
BuildAmigaOS4Offline can build and run the same offline checks from the project
drawer.

This package does not include Telegram tokens, SDK files or AmiSSL runtime
files.

Full notes are in README.md.
EOF

if command -v zip >/dev/null 2>&1; then
    (cd "$PACKAGE_ROOT" && rm -f "$DRAWER_NAME.zip" && zip -qr "$DRAWER_NAME.zip" "$DRAWER_NAME")
    echo "Created $PACKAGE_ROOT/$DRAWER_NAME.zip"
fi

echo "Created $DEST_DIR"
