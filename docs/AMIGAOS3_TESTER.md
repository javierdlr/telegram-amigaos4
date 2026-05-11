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
- run read-only stateful update checks;
- run inbox-format receive-only update checks with date/time, sender, message
  kind, compact one-line summaries and optional append-only logging;
- run one-shot, stateful batch or looped echo tests.

Certificate validation is not enabled yet. Use this build only for supervised
testing with test bots and disposable tokens.

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

The tester package also includes an AmigaDOS helper script:

```text
Execute RunAmigaOS3Preflight
```

Use `Execute`; running the script directly may require Amiga protection bits
that can be lost when unpacking ZIP archives. If you want to run it directly,
set them manually:

```text
Protect RunAmigaOS3Preflight +se
```

By default it auto-detects only `SYS:AmiSSL` and otherwise uses the existing
system `LIBS:`. It intentionally does not probe distribution-specific volumes,
because checking a missing volume can open an AmigaDOS requester on plain
systems. You can pass a custom AmiSSL drawer explicitly:

```text
Execute RunAmigaOS3Preflight AmiKit:Internet/AmiSSL telegram-test
```

## First Test Without A Token

Run offline parser and Bot API self-tests first:

```text
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-client-self-test
```

Run the preflight check:

```text
telegram-test --telegram-preflight
```

Or use the helper script:

```text
Execute RunAmigaOS3Preflight
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

Create a dedicated test bot before running live Bot API commands:

1. Open Telegram on a phone or desktop.
2. Search for `@BotFather`.
3. Send `/newbot`.
4. Choose a display name, for example `Telegram Amiga Test`.
5. Choose a username ending in `bot`, for example `MyAmigaTestBot`.
6. BotFather will return a token such as
   `123456789:REPLACE_WITH_REAL_SECRET`.

Create `telegram-token.txt` in the same drawer as `telegram-test`. Do not post
this file, do not commit it and do not paste the token in public logs.

You can create the file with your preferred Amiga text editor. It must contain
only one line, with no leading or trailing spaces:

```text
123456:REPLACE_WITH_REAL_BOT_TOKEN
```

The default commands read this file through `PROGDIR:telegram-token.txt`.

If a token is ever exposed in a screenshot, public log or forum post, revoke it
with BotFather using `/revoke` and create a new one.

## Bot API Smoke Tests

Check the token:

```text
telegram-test --telegram-getme-default
```

A valid token should return HTTP 200 and `telegram ok: true`.

Send a message to the bot from Telegram on your phone or desktop, for example:

```text
hello from phone
```

Then read updates on AmigaOS:

```text
telegram-test --telegram-get-updates-default
```

The output should include the update id, chat id and text. When several updates
are pending, the tester prints summaries for up to the first five.

Read pending updates and save the offset without sending replies:

```text
telegram-test --telegram-read-once-state-default telegram-offset.txt
```

This is the safest first receive test. It prints decoded message text and saves
`telegram-offset.txt` after each processed update.

Run bounded receive-only polling:

```text
telegram-test --telegram-read-loop-default telegram-offset.txt 5 10
```

This polls every five seconds for at most ten iterations and never sends
replies.

Run a more readable receive-only inbox:

```text
telegram-test --telegram-inbox-default telegram-offset.txt
telegram-test --telegram-inbox-loop-default telegram-offset.txt 5 10
telegram-test --inbox-log-file telegram-inbox.log --telegram-inbox-loop-default telegram-offset.txt 5 10
telegram-test --telegram-session-default telegram-offset.txt telegram-inbox.log telegram-chats.txt
telegram-test --telegram-session-loop-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
telegram-test --telegram-manual-client-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
telegram-test --telegram-client-default
```

Inbox output includes update id, chat id, sender name when available, decoded
text, message kind and the next saved offset. It uses the same offset file as
the read-only commands and never sends replies. Non-text messages currently
print placeholders such as `<photo>`, `<sticker>` or `<document>`.

`telegram-session-default` is the first manual-client preview. It reads pending
updates once, saves the next offset, appends readable inbox lines and updates
`telegram-chats.txt`. It does not send replies. The chat state format is:

```text
<chat-id>|<last-sender>|<last-date>|<last-text-or-placeholder>
```

Use `telegram-session-loop-default` for a bounded receive-only manual session
loop. Use `telegram-manual-client-default` for the current single-command text
preview: it performs bounded receive-only polling, updates the inbox/chat files
and then prints the saved chat list. It still never sends messages
automatically.

The shorter `telegram-client-default` command is the recommended manual test
entry point. It uses these default files in the program drawer:
`telegram-token.txt`, `telegram-offset.txt`, `telegram-inbox.log` and
`telegram-chats.txt`. Without timing arguments it polls every 5 seconds for 10
iterations; use `telegram-client-default 2 5` to override that timing.

List saved chats with:

```text
telegram-test --telegram-chats telegram-chats.txt
telegram-test --telegram-chats-default
```

This is the easiest way to avoid copying the raw `chat id` by hand. The
manual/session commands update `telegram-chats.txt` with the most recently
active chat at index `1`; then `--telegram-chats-default` prints a numbered
list, and `--telegram-reply-default` sends to the selected 1-based index.

Send a controlled message back using the 1-based chat list index:

```text
telegram-test --telegram-send-chat-default telegram-chats.txt 1 "Hello from AmigaOS 3.x"
telegram-test --telegram-reply-default 1 "Hello from AmigaOS 3.x"
telegram-test --telegram-send-last-default "Hello from AmigaOS 3.x"
```

Send a controlled message back:

```text
telegram-test --telegram-send-message-default <chat-id> "Hello from AmigaOS 3.x"
```

The shorter alias is also available:

```text
telegram-test --telegram-send-default <chat-id> "Hello from AmigaOS 3.x"
```

Use the `chat id` printed by `getUpdates` or `read-once-state` only when you
want the explicit chat-id command. For normal testing, prefer the saved chat
list flow above.

Run one stateful echo batch:

```text
telegram-test --telegram-echo-once-state-default telegram-offset.txt
```

One stateful batch processes up to five pending updates and saves
`telegram-offset.txt` after each handled update.

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
