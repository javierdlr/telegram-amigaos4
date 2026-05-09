<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AmigaOS 3.x Tester Notes

This document describes how to build and run the current AmigaOS 3.x diagnostic
tester. It is not a usable Telegram client yet. It is a pre-alpha technical
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
- run a bounded one-shot or looped echo test.

Certificate validation is not enabled yet. Use this build only for supervised
testing with test bots and disposable tokens.

## Target Requirements

The tested AmigaOS 3.x build currently requires:

- AmigaOS 3.x with a working TCP/IP stack;
- `ixemul.library`, because the current m68k GCC build links through ixemul;
- AmiSSL v5 for HTTPS tests;
- enough stack for TLS work, for example `Stack 65536`;
- a real or test Telegram bot token only for Bot API commands.

On Vampire or 68080-class systems, use the AmiSSL 68020/030/040/080 library
variant. If AmiSSL libraries were upgraded or changed while the system was
running, reboot or run `Avail FLUSH` before testing.

## Build From The Mac

Build the AmiSSL-enabled 68k binary:

```sh
PATH=/path/to/m68k-amigaos-toolchain/bin:$PATH \
make -f Makefile.amigaos3-gcc \
  ENABLE_AMISSL=1 \
  AMISSL_SDK=/path/to/AmiSSL/Developer \
  TARGET=build/amigaos3/telegram-test-amissl
```

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
match your installation:

```text
Assign AmiSSL: SYS:AmiSSL
Assign LIBS: AmiSSL:Libs ADD
Stack 65536
Avail FLUSH
```

If AmiSSL is already installed globally and working, the assigns may already be
present.

## First Test Without A Token

Run the preflight check:

```text
telegram-test --telegram-preflight
```

Expected successful HTTPS output includes:

```text
telegram preflight token file: PROGDIR:telegram-token.txt
telegram preflight token file: missing
telegram preflight https: received ...
telegram preflight http status: 302 Moved Temporarily
telegram preflight: ok
```

This confirms DNS, TCP, TLS and HTTP reachability to Telegram. It does not send
a token and does not call a Bot API method.

## Add A Bot Token

Create `telegram-token.txt` in the same drawer as `telegram-test`. Do not post
this file, do not commit it and do not paste the token in public logs.

You can create the file with your preferred Amiga text editor. It must contain
only one line:

```text
123456:REPLACE_WITH_REAL_BOT_TOKEN
```

The default commands read this file through `PROGDIR:telegram-token.txt`.

## Bot API Smoke Tests

Check the token:

```text
telegram-test --telegram-getme-default
```

A valid token should return HTTP 200 and `telegram ok: true`.

After sending a message to the bot from Telegram, read updates:

```text
telegram-test --telegram-get-updates-default
```

The output should include the update id, chat id and text. When several updates
are pending, the tester prints summaries for up to the first five.

Send a controlled message back:

```text
telegram-test --telegram-send-message-default <chat-id> "Hello from AmigaOS 3.x"
```

Run one stateful echo attempt:

```text
telegram-test --telegram-echo-once-state-default telegram-offset.txt
```

Run a bounded echo loop:

```text
telegram-test --telegram-echo-loop-default telegram-offset.txt 5 10
```

This polls every five seconds for at most ten iterations.

## Common Failures

TLS is disabled in the binary:

```text
telegram preflight https: failed: tls-error / unsupported
```

Use the AmiSSL-enabled build.

AmiSSL is not installed or not assigned correctly:

```text
telegram preflight https: failed: tls-error / handshake-failed
```

Check `AmiSSL:`, `LIBS:` and the installed AmiSSL version.

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

## Reporting Results

Useful reports include:

- Amiga model, accelerator and CPU;
- AmigaOS version;
- TCP/IP stack;
- AmiSSL version;
- whether `--telegram-preflight` reached HTTP 302;
- command output with tokens removed.

Do not publish token files, generated `/bot<token>/...` paths or screenshots
that show a real token.
