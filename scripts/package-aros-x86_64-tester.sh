#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Package helper for the AROS x86_64 offline pre-alpha tester. TLS is not
# available yet on this target because the validated SDK lacks OpenSSL headers
# and libraries.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PACKAGE_ROOT=${PACKAGE_ROOT:-"$ROOT_DIR/build/packages"}
TARGET=${TARGET:-"$ROOT_DIR/build/aros-x86_64/telegram-test"}
ENABLE_TLS=${ENABLE_TLS:-0}
DATE_STAMP=$(date +%Y%m%d)
COMMIT_ID=${COMMIT_ID:-$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)}
DRAWER_NAME="telegram-amiga-aros-x86_64-prealpha-$DATE_STAMP-$COMMIT_ID"
DEST_DIR="$PACKAGE_ROOT/$DRAWER_NAME"

if [ "${BUILD:-1}" = "1" ]; then
    make -C "$ROOT_DIR" -f Makefile.aros-x86_64 \
        clean all \
        ENABLE_TLS="$ENABLE_TLS" \
        TARGET="$TARGET"
fi

if [ ! -f "$TARGET" ]; then
    echo "AROS x86_64 binary not found: $TARGET" >&2
    echo "Build it first with Makefile.aros-x86_64 or pass TARGET=/path/to/telegram-test." >&2
    exit 1
fi

rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"

cp "$TARGET" "$DEST_DIR/telegram-test"
cp "$ROOT_DIR/docs/AROS_X86_64_TESTER.md" "$DEST_DIR/README.md"
cp "$ROOT_DIR/docs/HOW_TO_TEST.md" "$DEST_DIR/HOW_TO_TEST.md"
cp "$ROOT_DIR/docs/TLS_CERTIFICATES.md" "$DEST_DIR/TLS_CERTIFICATES.md"

cat > "$DEST_DIR/README.txt" <<EOF
Telegram Amiga - AROS x86_64 offline pre-alpha tester
Build: $COMMIT_ID
TLS enabled: $ENABLE_TLS

This target has been validated for offline self-tests on hosted AROS x86_64
through short non-interactive BebboSSHd commands. It is not a live Telegram
build yet because the current AROS x86_64 SDK used for this package does not
include OpenSSL headers or libraries.

Minimum offline test:

  telegram-test --help
  telegram-test --telegram-json-self-test
  telegram-test --telegram-get-updates-self-test
  telegram-test --telegram-read-once-state-self-test
  telegram-test --telegram-inbox-self-test
  telegram-test --telegram-echo-once-self-test
  telegram-test --telegram-send-message-self-test
  telegram-test --telegram-console-self-test
  telegram-test --telegram-client-self-test
  telegram-test --telegram-tls-status

If built with TLS enabled, use only disposable test bot tokens until HTTPS has
been validated repeatedly on the target.

For this package, TLS is expected to be disabled. Commands that need HTTPS or
Telegram live API access are expected to report unsupported until the x86_64
OpenSSL SDK path is available.

Hosted AROS x86_64 runtime notes:

  - 10.255.222.2:2222 is a TAP-internal endpoint on the Linux build server.
  - It works only while hosted AROS is running, and normally only from that
    server unless an SSH tunnel is used.
  - Use BebboSSHd x64 v0.3.1 or newer. Older x64 builds used too small a
    command stack for heavier telegram-test self-tests.
  - Use short non-interactive commands. Do not use shell redirection or pipes.
  - Do not rely on interactive console mode through this SSH path yet.

Full notes are in README.md. The common checklist is in HOW_TO_TEST.md.
TLS validation details are in TLS_CERTIFICATES.md.
EOF

if command -v zip >/dev/null 2>&1; then
    (cd "$PACKAGE_ROOT" && rm -f "$DRAWER_NAME.zip" && zip -qr "$DRAWER_NAME.zip" "$DRAWER_NAME")
    echo "Created $PACKAGE_ROOT/$DRAWER_NAME.zip"
fi

echo "Created $DEST_DIR"
