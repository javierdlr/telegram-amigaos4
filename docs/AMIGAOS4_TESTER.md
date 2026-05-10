<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AmigaOS 4.x Tester Notes

This document describes how to build and run the current AmigaOS 4.x
diagnostic tester. It is not a usable Telegram client yet. It is a pre-alpha
technical build for checking startup, local parsers, AmiSSL, HTTPS reachability
and a small set of Telegram Bot API commands.

## Current Scope

The AmigaOS 4.x tester can:

- print the platform and data directory;
- run local parser and HTTP self-tests;
- check the default token file and HTTPS reachability with
  `--telegram-preflight`;
- call Telegram Bot API `getMe`, `getUpdates` and `sendMessage` when a token is
  provided;
- run read-only stateful update checks;
- run inbox-format receive-only update checks with date/time, sender, message
  kind, compact one-line summaries and optional append-only logging;
- run the current manual-client preview commands;
- send a controlled message to a saved chat by chat-list index.

Certificate validation is not enabled yet. Use this build only for supervised
testing with test bots and disposable tokens.

## Tested Target

The current AmigaOS 4.x work was verified on a QEMU target reachable through
BebboSSH port forwarding.

```text
Kickstart 54.57, Workbench 53.21
```

The confirmed setup used:

- AmigaOS 4 SDK 54.16 installed in `Work:SDK`;
- GCC 11.2.0 from the SDK;
- AmiSSL runtime with `amisslmaster.library 5.25 (12-Oct-2025)`;
- AmiSSL SDK package `AmiSSL-5.3.lha`;
- `execsg_sdk-54.31.lha`, required for `proto/exec.h`;
- the SDK startup persisted from `S:User-Startup`.

## Target Requirements

The tested AmigaOS 4.x build currently requires:

- AmigaOS 4.x with a working TCP/IP stack;
- AmiSSL v5 installed and visible through `LIBS:` for HTTPS tests;
- a real or test Telegram bot token only for live Bot API commands;
- enough stack for TLS work. If in doubt, run `Stack 65536` before tests.

For source builds on AmigaOS 4.x, also install the SDK packages listed below:

- base SDK;
- `execsg_sdk-54.31.lha`;
- `gcc-noarch.lha`;
- one GCC binary package, currently tested with `gcc-11.2.0-bin.lha`;
- `newlib-53.80.lha`;
- `AmiSSL-5.3.lha` for AmiSSL headers and `libamisslstubs.a`.

If AmiSSL libraries were upgraded or changed while the system was running,
reboot or run `Avail FLUSH` before testing.

## Build On AmigaOS 4.x

From the project drawer, first activate the SDK:

```text
Assign SDK: Work:SDK
Execute SDK:S/sdk-startup
```

Build the offline tester:

```text
make -f Makefile.amigaos4 clean all
```

Build the AmiSSL-enabled HTTPS tester:

```text
make -f Makefile.amigaos4 clean all ENABLE_AMISSL=1
```

If `make` is not available but `gcc` is available, use the included AmigaDOS
helper from the project drawer:

```text
Execute scripts/BuildAmigaOS4Offline
```

The helper compiles the current offline tester and runs the parser/inbox/send
self-tests.

## Create A Package From The Mac

The package script expects an AmigaOS 4.x binary already built on the AmigaOS
4.x target and copied back to the Mac:

```sh
scripts/package-amigaos4-tester.sh
```

By default it reads `build/amigaos4/telegram-test` and writes the package under
`build/packages/`, which is ignored by git. The package does not include
Telegram tokens, SDK files or AmiSSL runtime files.

## Install On AmigaOS 4.x

Copy the package drawer to the AmigaOS 4.x system and enter it:

```text
CD Work:TelegramAmiga
```

If AmiSSL is already installed globally and working, no extra assigns should be
needed. If AmiSSL is installed in a separate drawer, adjust the path to match
your installation:

```text
Assign AmiSSL: SYS:AmiSSL
Assign LIBS: AmiSSL:Libs ADD
Assign LIBS: SYS:Libs ADD
Stack 65536
Avail FLUSH
```

## First Test Without A Token

Run offline parser and Bot API self-tests first:

```text
telegram-test --help
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
```

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

Then read updates on AmigaOS 4.x:

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

List saved chats with:

```text
telegram-test --telegram-chats telegram-chats.txt
```

Send a controlled message back using the 1-based chat list index:

```text
telegram-test --telegram-send-chat-default telegram-chats.txt 1 "Hello from AmigaOS 4.x"
```

The explicit chat-id send command is still available:

```text
telegram-test --telegram-send-default <chat-id> "Hello from AmigaOS 4.x"
```

## Confirmed Live Results

On the QEMU AmigaOS 4.x target, the AmiSSL build has been verified with:

- `--telegram-preflight`: HTTP 302 and ok;
- `--telegram-getme`: HTTP 200 and ok for `KaffoAmigaBot`;
- `--telegram-read-loop`: one read-only iteration, rc 0, no update available.

## Reporting Results

When reporting AmigaOS 4.x test results, include:

- AmigaOS 4.x version;
- whether the build used `ENABLE_AMISSL=0` or `ENABLE_AMISSL=1`;
- compiler version and SDK source if built locally;
- AmiSSL version if TLS was enabled;
- exact command output from the self-tests, preflight or live checks;
- whether the token was a disposable test bot token.

Do not include Telegram tokens or screenshots containing tokens.
