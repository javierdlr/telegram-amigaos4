<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AmigaOS 3.x Tester Notes

This document describes how to build and run the current AmigaOS 3.x diagnostic
tester. It includes a pre-alpha manual text client and remains a technical
build for checking startup, AmiSSL, HTTPS reachability and a small set of
Telegram Bot API commands.

## Current Scope

The AmigaOS 3.x tester can:

- print the platform and data directory;
- run local parser and HTTP self-tests;
- check the default token file and HTTPS reachability with
  `--telegram-preflight`;
- call Telegram Bot API `getMe`, `getUpdates` and `sendMessage` when a token is
  provided;
- run read-only stateful update checks;
- run inbox-format receive-only update checks with date/time, sender, message
  kind, compact one-line summaries and optional append-only logging;
- run one-shot, stateful batch or looped echo tests.

AmiSSL certificate validation is opt-in with `--tls-verify` and either a CA
bundle supplied through `--tls-ca-file` or a usable AmiSSL/OpenSSL default
trust store. This has passed a supervised HTTPS smoke test on the project
AmigaOS 3.x/Vampire setup with an explicit CA bundle. Keep using test bots and
disposable tokens until this path has more independent target-side testing.

## Target Requirements

The tested AmigaOS 3.x build currently requires:

- AmigaOS 3.x with a working TCP/IP stack;
- `ixemul.library`, because the current m68k GCC build links through ixemul;
- AmiSSL 5.18 or newer for HTTPS tests;
- enough stack for TLS work, for example `Stack 65536`;
- a real or test Telegram bot token only for Bot API commands.

On 68060 systems, use the AmiSSL `68060` library variant. On Vampire or
68080-class systems, use the AmiSSL `68020/030/040/080` library variant. If
AmiSSL libraries were upgraded or changed while the system was running, reboot
or run `Avail FLUSH` before testing.

## Build From The Mac

Build the AmiSSL-enabled 68k binary:

```sh
PATH=/path/to/m68k-amigaos-toolchain/bin:$PATH \
make -f Makefile.amigaos3-gcc \
  ENABLE_AMISSL=1 \
  AMISSL_SDK=/path/to/AmiSSL/Developer \
  TARGET=build/amigaos3/telegram-test-amissl
```

By default this asks AmiSSL for the conservative `AMISSL_V340` API, matching
AmiSSL 5.18 or newer. This keeps public tester binaries aligned with normal
system AmiSSL installations. To intentionally require a newer runtime, override
`AMISSL_API_VERSION`, for example `AMISSL_API_VERSION=AMISSL_V360`.

Or create a local tester package:

```sh
scripts/package-amigaos3-tester.sh
```

The package is written under `build/packages/`, which is ignored by git. The
script always creates a drawer and creates a `.zip` when `zip` is available.

## Install On AmigaOS 3.x

Copy the package drawer to the Amiga and enter it:

```text
CD Work:TelegramAmiga
```

Make sure AmiSSL is assigned and visible through `LIBS:`. Adjust the path to
match your installation. If AmiSSL is already installed globally and working,
you may not need these assigns:

```text
Assign AmiSSL: SYS:AmiSSL
Assign LIBS: AmiSSL:Libs
Assign LIBS: SYS:Libs ADD
Stack 65536
Avail FLUSH
```

The tester package also includes AmigaDOS helper scripts:

```text
Execute RunAmigaOS3Preflight
Execute RunAmigaOS3GetMe
Execute RunAmigaOS3HumanChat
Execute TelegramAmiga
Execute RunMTProtoStart
Execute RunMTProtoLoginWizard
Execute RunMTProtoLoginSmoke
Execute RunMTProtoListPeers
Execute RunMTProtoChat
```

Use `Execute`; the helpers set stack and restore the executable bit on
`telegram-test` after ZIP extraction. Running the scripts directly may require
Amiga protection bits that can be lost when unpacking ZIP archives. If you want
to run them directly, set them manually:

```text
Protect RunAmigaOS3Preflight +se
Protect RunAmigaOS3GetMe +se
Protect RunAmigaOS3HumanChat +se
Protect RunMTProtoLoginWizard +se
Protect RunMTProtoChat +se
```

By default it auto-detects only `SYS:AmiSSL` and otherwise uses the existing
system `LIBS:`. It intentionally does not probe distribution-specific volumes,
because checking a missing volume can open an AmigaDOS requester on plain
systems. You can pass a custom AmiSSL drawer explicitly:

```text
Execute RunAmigaOS3Preflight AmiKit:Internet/AmiSSL telegram-test
```

## Common test walkthrough

For the shared Bot-API command walkthrough — offline self-tests, creating a test bot, reading messages, human chat, the manual console and reporting results — see `HOW_TO_TEST.md` and `USER_RUNBOOK.md` (included in this package).

## MTProto Account Login And User Chat

The package also contains a pre-alpha MTProto account login path. This is
separate from the Bot API tester and can send real Telegram messages to normal
users selected from the account's dialog list.

Create `telegram-api.txt` in the package drawer:

```text
<api_id>
<api_hash>
```

Keep these MTProto files private and out of screenshots, archives and forum
posts:

```text
telegram-api.txt
telegram-auth.bin
phone-code-hash.txt
telegram-password.txt
telegram-peers.txt
```

Start the MTProto client:

```text
Execute TelegramAmiga
Execute RunMTProtoStart
```

If no saved login exists, this starts the phone/code login wizard first. After
login it uses the DC stored in `telegram-auth.bin`, refreshes the peer cache
and enters chat mode.

Release packages include an IconX project icon named `TelegramAmiga.info`
next to `TelegramAmiga`. Double-click `TelegramAmiga` from Workbench to start
the same flow. If icon metadata is lost during unpacking, use
`Execute TelegramAmiga` from Shell.

Manual login is still available:

```text
Execute RunMTProtoLoginWizard
```

The wizard asks for phone number, Telegram login code and optional 2FA
password. Some Amiga console setups can echo password input, so avoid running
that step during public screen-sharing.

Manual validation/debug commands:

```text
Execute RunMTProtoCheckLocal
Execute RunMTProtoInspectAuth
Execute RunMTProtoLoginSmoke
Execute RunMTProtoListPeers
```

Manual chat entry:

```text
Execute RunMTProtoChat
```

Pick a peer index and type normal text to send. User peers, basic groups and channels/supergroups use the same text mode when cached peer data is available. Incoming peer messages are
auto-read every 2 seconds while waiting for input. `/read` polls immediately,
`/watch <seconds>` changes the interval, `/watch off` disables auto-read,
`/peer` changes peer, `/peers` refreshes the peer cache and `/quit` exits.

If you see `auth-dc-mismatch`, inspect the saved auth file and use the matching
DC endpoint explicitly. The latest live AmigaOS 3.x validation used:

```text
Execute RunMTProtoChat 149.154.167.91 443 4 telegram-api.txt telegram-auth.bin telegram-peers.txt telegram-test
```

## Common Failures

TLS is disabled in the binary:

```text
telegram preflight https: failed: tls-error / unsupported
```

Use the AmiSSL-enabled build.

AmiSSL is not installed, not visible through `LIBS:`, or too old for the binary:

```text
telegram preflight https: failed: tls-error / handshake-failed
```

Check `LIBS:amisslmaster.library`, the installed AmiSSL version and whether the
binary was built for a compatible `AMISSL_API_VERSION`.

The token file is missing:

```text
telegram getMe: failed: token-error / file-error / open-failed
```

Create `telegram-token.txt` in the program drawer or use `--token-file <path>`.

The token is wrong or revoked:

```text
telegram http status: 401
telegram ok: false
telegram description: Unauthorized
```

Revoke and recreate the token with BotFather if it was exposed.
