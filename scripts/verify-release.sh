#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Post-publish check: download each GitHub release asset and confirm it matches
# the locally-built binary (md5), is the right architecture, leaks no session
# file, ships the flashless icon and the expected files. Catches a stale upload
# or a missed --clobber. Usage: VERSION=0.0.2 sh scripts/verify-release.sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=${REPO:-kaffeine1/telegram-amiga}
VERSION=${VERSION:-0.0.2}

md5of() { if command -v md5 >/dev/null 2>&1; then md5 -q "$1"; else md5sum "$1" | awk '{print $1}'; fi; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0

verify() {
    tag=$1; zippat=$2; localbin=$3; archpat=$4
    if [ ! -f "$ROOT_DIR/$localbin" ]; then echo "SKIP $tag: no local build $localbin"; return; fi
    if ! gh release download "$tag" --repo "$REPO" --pattern "$zippat" -O "$TMP/$tag.zip" >/dev/null 2>&1; then
        echo "FAIL $tag: download failed"; fail=1; return
    fi
    unzip -p "$TMP/$tag.zip" "*/telegram-test" > "$TMP/bin" 2>/dev/null
    want=$(md5of "$ROOT_DIR/$localbin"); got=$(md5of "$TMP/bin")
    arch=$(file "$TMP/bin" | grep -c "$archpat" || true)
    leak=$(unzip -l "$TMP/$tag.zip" | grep -icE "telegram-(auth|peers|seed|password|token)|phone-code-hash" || true)
    icon=$(unzip -p "$TMP/$tag.zip" "*/TelegramGUI.info" 2>/dev/null | strings | grep -c "^telegram-test$" || true)
    files=$(unzip -l "$TMP/$tag.zip" | grep -cE "Manual-EN.txt|Manuale-IT.txt|TelegramGUI$|TelegramTUI$|telegram-api.txt" || true)
    ok=1
    [ "$want" = "$got" ] || { echo "FAIL $tag: published binary $got != local build $want"; ok=0; }
    [ "$arch" -ge 1 ]    || { echo "FAIL $tag: wrong architecture"; ok=0; }
    [ "$leak" = 0 ]      || { echo "FAIL $tag: SESSION FILE LEAK"; ok=0; }
    [ "$icon" -ge 1 ]    || { echo "FAIL $tag: TelegramGUI.info not flashless"; ok=0; }
    [ "$files" = 5 ]     || { echo "FAIL $tag: $files/5 expected files"; ok=0; }
    if [ "$ok" = 1 ]; then echo "OK   $tag  [bin $(printf %.8s "$got")]"; else fail=1; fi
}

verify "os3-alpha-$VERSION"        "*amigaos3*"   build/amigaos3-clib2/telegram-test  "AmigaOS"
verify "os4-alpha-$VERSION"        "*amigaos4*"   build/amigaos4/telegram-test        "PowerPC"
verify "morphos-alpha-$VERSION"    "*morphos*"    build/morphos-cross/telegram-test   "PowerPC"
verify "aros-i386-alpha-$VERSION"  "*aros-i386*"  build/aros-i386-abiv0/telegram-test "80386"
verify "aros-x86_64-alpha-$VERSION" "*x86_64*"    build/aros-x86_64/telegram-test     "x86-64"

if [ "$fail" = 0 ]; then
    echo "All published $VERSION assets match the local builds."
else
    echo "VERIFICATION FAILED -- re-upload the offending asset(s)." >&2
    exit 1
fi
