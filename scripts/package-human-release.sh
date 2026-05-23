#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Build minimal human-facing packages. These artifacts intentionally contain
# only the program, one Workbench/Ambient/Wanderer launcher, its icon and one
# user README.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PACKAGE_ROOT=${PACKAGE_ROOT:-"$ROOT_DIR/build/human-releases"}
DATE_STAMP=${DATE_STAMP:-$(date +%Y%m%d)}
COMMIT_ID=${COMMIT_ID:-$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)}

AMIGAOS3_BINARY=${AMIGAOS3_BINARY:-"$ROOT_DIR/build/amigaos3/telegram-test-amissl"}
MORPHOS_BINARY=${MORPHOS_BINARY:-"$ROOT_DIR/build/morphos-cross/telegram-test"}
AMIGAOS4_BINARY=${AMIGAOS4_BINARY:-"$ROOT_DIR/build/amigaos4/telegram-test"}
AROS_I386_BINARY=${AROS_I386_BINARY:-"$ROOT_DIR/build/aros-i386-abiv0/telegram-test"}
AROS_X86_64_BINARY=${AROS_X86_64_BINARY:-"$ROOT_DIR/build/aros-x86_64/telegram-test"}

mkdir -p "$PACKAGE_ROOT"

write_launcher() {
    launcher_path=$1
    cat > "$launcher_path" <<'EOF'
; Telegram Amiga human launcher.

FailAt 21
Stack 65536

Echo "Telegram Amiga"
Echo "Author: Michele Dipace <michele.dipace@kaffeine.net>"

IF EXISTS telegram-api.txt
    SET APIFILE "telegram-api.txt"
ELSE
    SET APIFILE "RAM:telegram-api.txt"
ENDIF

IF EXISTS telegram-auth.bin
    SET AUTHFILE "telegram-auth.bin"
ELSE
    SET AUTHFILE "RAM:telegram-auth.bin"
ENDIF

IF EXISTS phone-code-hash.txt
    SET HASHFILE "phone-code-hash.txt"
ELSE
    SET HASHFILE "RAM:phone-code-hash.txt"
ENDIF

IF EXISTS telegram-peers.txt
    SET PEERSFILE "telegram-peers.txt"
ELSE
    SET PEERSFILE "RAM:telegram-peers.txt"
ENDIF

IF EXISTS telegram-test
    Protect telegram-test +e
    telegram-test --mtproto-start-file "$APIFILE" "$AUTHFILE" "$HASHFILE" "$PEERSFILE"
ELSE
    Protect RAM:telegram-test +e
    RAM:telegram-test --mtproto-start-file "$APIFILE" "$AUTHFILE" "$HASHFILE" "$PEERSFILE"
ENDIF

IF WARN
    Echo ""
    Echo "Telegram Amiga ended. Read any message above."
    Echo "This window will stay open for 120 seconds."
    Wait 120
ENDIF
EOF
}

write_readme() {
    readme_path=$1
    readme_platform=$2
    cat > "$readme_path" <<EOF
Telegram Amiga
==============

Platform: $readme_platform
Build: $COMMIT_ID

Telegram Amiga is an early text-mode Telegram client for Amiga-like systems.
This package is meant for human testing: open the drawer and double-click the
TelegramAmiga icon.

Included files
--------------

- telegram-test: the client binary
- TelegramAmiga: the launcher used by the icon
- TelegramAmiga.info: Workbench/Ambient/Wanderer project icon
- README.txt: this guide

Private files you create locally
--------------------------------

These files are NOT included and must never be published:

- telegram-api.txt
- telegram-auth.bin
- phone-code-hash.txt
- telegram-password.txt
- telegram-peers.txt
- telegram-token.txt

telegram-auth.bin is especially sensitive. It contains the saved MTProto
authorization for a Telegram account. Sharing it can give someone else access
to that logged-in session.

First start
-----------

1. Create telegram-api.txt in the same drawer, or copy it to RAM:.
   It must contain exactly:

   <api_id>
   <api_hash>

2. Double-click TelegramAmiga.

3. If no saved login exists, the login wizard starts automatically.
   Enter the phone number, then the Telegram login code when requested.
   If Telegram asks for a 2FA password, enter it only on a private screen.

4. After login, choose a chat number.

Using chat
----------

Type text and press Enter to send to the selected chat.

Useful commands:

  /read         read recent messages
  /peer         choose another chat
  /peer <n>     switch to chat number n
  /peers        refresh chat list
  /watch <sec>  change auto-read interval
  /watch off    disable auto-read
  /help         show commands
  /quit         exit

Notes
-----

Private-message sends may be refused by Telegram with PEER_FLOOD for new or
restricted accounts. If that happens, stop retrying for a while. Group chats
can still be used as the write test path.

Keep screenshots clean: do not show phone numbers, login codes, passwords,
tokens, telegram-api.txt, telegram-auth.bin, or private messages.
EOF
}

package_one() {
    platform=$1
    binary=$2
    suffix=$3
    expected=$4

    if [ ! -f "$binary" ]; then
        echo "Skipping $platform: binary not found: $binary" >&2
        return 0
    fi

    file_output=$(file "$binary")
    case "$expected" in
        amigaos3)
            echo "$file_output" | grep -q "AmigaOS loadseg" || {
                echo "Skipping $platform: unexpected binary: $file_output" >&2
                return 0
            }
            ;;
        morphos)
            echo "$file_output" | grep -q "ELF 32-bit MSB relocatable, PowerPC" || {
                echo "Skipping $platform: unexpected binary: $file_output" >&2
                return 0
            }
            ;;
        amigaos4)
            echo "$file_output" | grep -q "ELF 32-bit MSB executable, PowerPC" || {
                echo "Skipping $platform: unexpected binary: $file_output" >&2
                return 0
            }
            ;;
        aros-i386)
            echo "$file_output" | grep -q "ELF 32-bit LSB relocatable, Intel 80386.*AROS" || {
                echo "Skipping $platform: unexpected binary: $file_output" >&2
                return 0
            }
            ;;
        aros-x86_64)
            echo "$file_output" | grep -q "ELF 64-bit LSB relocatable, x86-64.*AROS" || {
                echo "Skipping $platform: unexpected binary: $file_output" >&2
                return 0
            }
            ;;
        *)
            echo "Unknown expected type: $expected" >&2
            exit 1
            ;;
    esac

    drawer="telegram-amiga-$suffix-human-$DATE_STAMP-$COMMIT_ID"
    dest="$PACKAGE_ROOT/$drawer"
    rm -rf "$dest"
    mkdir -p "$dest"

    cp "$binary" "$dest/telegram-test"
    write_launcher "$dest/TelegramAmiga"
    cp "$ROOT_DIR/assets/TelegramAmiga.info" "$dest/TelegramAmiga.info"
    write_readme "$dest/README.txt" "$platform"

    if command -v zip >/dev/null 2>&1; then
        (cd "$PACKAGE_ROOT" && rm -f "$drawer.zip" && zip -qr "$drawer.zip" "$drawer")
        echo "$PACKAGE_ROOT/$drawer.zip"
    else
        echo "$dest"
    fi
}

package_one "AmigaOS 3.x" "$AMIGAOS3_BINARY" "amigaos3" "amigaos3"
package_one "MorphOS" "$MORPHOS_BINARY" "morphos" "morphos"
package_one "AmigaOS 4.x" "$AMIGAOS4_BINARY" "amigaos4" "amigaos4"
package_one "AROS i386 ABIv0" "$AROS_I386_BINARY" "aros-i386-abiv0" "aros-i386"
package_one "AROS x86_64" "$AROS_X86_64_BINARY" "aros-x86_64" "aros-x86_64"
