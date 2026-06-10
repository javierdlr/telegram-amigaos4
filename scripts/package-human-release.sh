#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Build minimal human-facing packages. These artifacts intentionally contain
# only the program, one Workbench/Ambient/Wanderer launcher, its icon, the
# public Telegram API app credentials and one user README.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PACKAGE_ROOT=${PACKAGE_ROOT:-"$ROOT_DIR/build/human-releases"}
DATE_STAMP=${DATE_STAMP:-$(date +%Y%m%d)}
COMMIT_ID=${COMMIT_ID:-$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)}

# Default to the AmiSSL-free m68k build (in-tree crypto, no AmiSSL needed).
# Default to the no-ixemul (clib2) m68k build: in-tree crypto, no AmiSSL and no
# ixemul.library required at runtime. Build it with Makefile.amigaos3-clib2.
AMIGAOS3_BINARY=${AMIGAOS3_BINARY:-"$ROOT_DIR/build/amigaos3-clib2/telegram-test"}
MORPHOS_BINARY=${MORPHOS_BINARY:-"$ROOT_DIR/build/morphos-cross/telegram-test"}
AMIGAOS4_BINARY=${AMIGAOS4_BINARY:-"$ROOT_DIR/build/amigaos4/telegram-test"}
AROS_I386_BINARY=${AROS_I386_BINARY:-"$ROOT_DIR/build/aros-i386-abiv0/telegram-test"}
AROS_X86_64_BINARY=${AROS_X86_64_BINARY:-"$ROOT_DIR/build/aros-x86_64/telegram-test"}

mkdir -p "$PACKAGE_ROOT"
# Short community-facing names: Telegram-<platform>-<date>.zip (the commit id is
# kept inside README.txt as "Build:" and in the GitHub tag, not in the filename).
rm -f "$PACKAGE_ROOT"/Telegram-*-"$DATE_STAMP".zip
rm -rf "$PACKAGE_ROOT"/Telegram-*-"$DATE_STAMP"
# Also clear any leftovers from the previous long naming scheme.
rm -f "$PACKAGE_ROOT"/telegram-amiga-*-human-"$DATE_STAMP"-*.zip
rm -rf "$PACKAGE_ROOT"/telegram-amiga-*-human-"$DATE_STAMP"-*

write_launcher() {
    launcher_path=$1
    cat > "$launcher_path" <<'EOF'
; Telegram Amiga human launcher.

FailAt 21
Stack 262144
.BRA {
.KET }

Echo "Telegram Amiga"
Echo "Author: Michele Dipace <michele.dipace@kaffeine.net>"

SET APIFILE "telegram-api.txt"
SET AUTHFILE "telegram-auth.bin"
SET HASHFILE "phone-code-hash.txt"
SET PEERSFILE "telegram-peers.txt"

IF EXISTS telegram-test
    Protect telegram-test +e
    telegram-test --mtproto-start-file "$APIFILE" "$AUTHFILE" "$HASHFILE" "$PEERSFILE" <* >*
ELSE
    Protect RAM:telegram-test +e
    RAM:telegram-test --mtproto-start-file "$APIFILE" "$AUTHFILE" "$HASHFILE" "$PEERSFILE" <* >*
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

    case "$readme_platform" in
    "AmigaOS 3.x")
        readme_requirements="System requirements
-------------------

- AmigaOS 3.x (3.1 / 3.1.4 / 3.2) with a working TCP/IP stack
  (Roadshow, AmiTCP, Miami/MiamiDx) providing bsdsocket.library, plus a
  working internet connection.
- A 68020 or better CPU. This build uses 68020 instructions (hardware 32x32
  multiply) and will NOT run on a plain 68000. Tested on 68080/Vampire;
  community testing on 68020/030/040/060 is welcome.
- A few MB of free RAM; the launcher requests Stack 262144.

NO ixemul.library and NO AmiSSL needed: this is a native build (clib2 C
runtime, bsdsocket.library directly) with all cryptography (RSA,
Diffie-Hellman, SRP 2FA, AES, SHA) built in. Just unpack and double-click. The
first login does some heavy big-number math and may take a minute on slower
68k CPUs; this is a one-time cost (the saved login is reused afterwards) and
normal chatting stays fast.
"
        ;;
    *)
        readme_requirements="System requirements
-------------------

- $readme_platform with a working TCP/IP stack (bsdsocket) and a working
  internet connection.
- A few MB of free RAM; the launcher requests Stack 262144.
"
        ;;
    esac

    cat > "$readme_path" <<EOF
Telegram Amiga
==============

Platform: $readme_platform
Build: $COMMIT_ID

Telegram Amiga is an early text-mode Telegram client for Amiga-like systems.
This package is meant for human testing: open the drawer and double-click the
TelegramAmiga icon.

$readme_requirements
Included files
--------------

- telegram-test: the client binary
- TelegramAmiga: the launcher used by the icon
- TelegramAmiga.info: Workbench/Ambient/Wanderer project icon
- telegram-api.txt: public Telegram API app id/hash for TelegramAmiga
- README.txt: this guide

Private files created locally
-----------------------------

These files are NOT included and must never be published:

- telegram-auth.bin
- phone-code-hash.txt
- telegram-password.txt
- telegram-peers.txt
- telegram-token.txt

telegram-auth.bin is especially sensitive. It contains the saved MTProto
authorization for a Telegram account. Sharing it can give someone else access
to that logged-in session.

How telegram-auth.bin is created
--------------------------------

Do not download telegram-auth.bin from anyone and do not copy another user's
file. TelegramAmiga creates it locally on your Amiga-like system after a
successful login.

On first start:

1. TelegramAmiga uses the bundled telegram-api.txt.
2. It asks for your phone number.
3. Telegram sends a login code to your Telegram account.
4. You type that code into TelegramAmiga.
5. If Telegram asks for 2FA, you type the password locally.
6. TelegramAmiga writes telegram-auth.bin in this drawer.

After that, the same telegram-auth.bin is reused so the wizard does not ask for
the phone/code every time.

First start
-----------

1. Double-click TelegramAmiga.

2. If no saved login exists, the login wizard starts automatically.
   Enter the phone number, then the Telegram login code when requested.
   If Telegram asks for a 2FA password, enter it only on a private screen.

3. After login, choose a chat number.

Advanced API override
---------------------

The bundled telegram-api.txt is meant for normal users. Advanced testers may
replace it with their own Telegram API id/hash file using the same two-line
format:

  <api_id>
  <api_hash>

Using chat
----------

Type text and press Enter to send to the selected chat. New messages from the
open chat appear automatically every few seconds, even while you are typing.

Switching chats is fast:

  F1..F10       jump straight to chat 1..10 from the list
                (Shift+F1..F10 reaches chats 11..20)
  Tab           jump back to the previous chat
  <number>      type a chat number and press Enter
  /swap         same as Tab

Commands:

  /peers        show the chat list (the open chat is marked with *)
  /search text  find a cached chat by name
  /add name     search Telegram and add a new chat
  /remove n     remove chat n from the list
  /history      show recent messages again
  /watch sec    change the auto-read interval (/watch off disables it)
  /color        toggle colours (also /color on or /color off)
  /help         show commands
  /quit         exit

Up/Down arrows recall what you typed before.

How it looks
------------

The chat uses a black high-contrast screen by default: sender names are bold
(your contact in the accent colour, you in white), system messages are dim,
and multi-line messages keep their line breaks. Emoji are shown as classic
text emoticons like :) :D <3 (y) because Amiga consoles cannot draw emoji
glyphs; flags become their two country letters (IT, DE) and other symbols a
small generic mark. A bold [ok] confirms every sent message.

Prefer the normal window colours? Start the client with --ui-theme plain, or
type /color off inside the chat.

The text size is the system console font: make it bigger in the OS font
preferences (AmigaOS 4: Prefs -> Fonts; AmigaOS 3: Prefs -> Font; MorphOS and
AROS: font preferences) and TelegramAmiga follows it.

Notes
-----

Private-message sends may be refused by Telegram with PEER_FLOOD for new or
restricted accounts. If that happens, stop retrying for a while. Group chats
can still be used as the write test path.

If typed keys do not match the characters shown on screen, check the operating
system keymap first. TelegramAmiga reads from the system console; keyboard
layout is handled by the Amiga-like OS, not by the client. Set the correct
layout in the system input preferences or startup files before testing accented
characters.

Keep screenshots clean: do not show phone numbers, login codes, passwords,
tokens, telegram-auth.bin, or private messages.
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

    drawer="Telegram-$suffix-$DATE_STAMP"
    dest="$PACKAGE_ROOT/$drawer"
    rm -rf "$dest"
    mkdir -p "$dest"

    cp "$binary" "$dest/telegram-test"
    write_launcher "$dest/TelegramAmiga"
    # Use the richer 64x64 colour icon (originally the OS4 one) on every platform.
    # It is a standard project icon (do_Type=4, default tool C:IconX, same launch)
    # with an appended colour icon, and carries a planar fallback, so OS3.x /
    # MorphOS / AROS / OS4 all show it. Falls back to the basic icon if missing.
    if [ -f "$ROOT_DIR/assets/TelegramAmigaOS4.info" ]; then
        cp "$ROOT_DIR/assets/TelegramAmigaOS4.info" "$dest/TelegramAmiga.info"
    else
        cp "$ROOT_DIR/assets/TelegramAmiga.info" "$dest/TelegramAmiga.info"
    fi
    cp "$ROOT_DIR/assets/public-telegram-api.txt" "$dest/telegram-api.txt"
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
package_one "AROS i386 ABIv0" "$AROS_I386_BINARY" "aros-i386" "aros-i386"
package_one "AROS x86_64" "$AROS_X86_64_BINARY" "aros-x86_64" "aros-x86_64"
