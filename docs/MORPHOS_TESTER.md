<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# MorphOS Tester Notes

This document describes how to build and run the current MorphOS diagnostic
tester. It is not a usable Telegram client yet. It is a pre-alpha technical
build for checking startup, local parsers, Bot API response handling and HTTPS
reachability when a TLS-enabled build is used.

## Current Scope

The MorphOS tester can:

- print the platform and data directory;
- run local parser and HTTP self-tests;
- parse Telegram Bot API `getMe`, `getUpdates` and `sendMessage` samples;
- run read-only and echo state self-tests;
- with a TLS-enabled build, call real Telegram Bot API commands when a token is
  provided;
- print pending updates in inbox format without sending replies, including
  date/time, sender, message kind, compact one-line summaries and optional
  append-only logging.

Certificate validation is not enabled yet. Use this build only for supervised
testing with test bots and disposable tokens.

## Target Requirements

The tested MorphOS build currently requires:

- MorphOS with the GG development environment for source builds;
- a working TCP/IP stack;
- OpenSSL development/runtime libraries for `ENABLE_TLS=1` builds;
- a real or test Telegram bot token only for live Bot API commands.

## Build On MorphOS

From the project drawer:

```text
System:Development/gg/bin/make -f Makefile.morphos clean all
```

The default source build has TLS disabled and is suitable for offline self-tests.
The public MorphOS tester package is built with TLS enabled.

To build the TLS-enabled tester:

```text
System:Development/gg/bin/make -f Makefile.morphos clean all ENABLE_TLS=1
```

If OpenSSL headers or libraries are missing, keep `ENABLE_TLS=0` and report the
MorphOS/OpenSSL setup details. On the tested MorphOS machine, `ENABLE_TLS=1`
successfully reached Telegram over HTTPS.

## Create A Package From The Mac

The package script expects a MorphOS binary already built on the MorphOS target
and copied back to the Mac:

```sh
scripts/package-morphos-tester.sh
```

By default it reads `build/morphos/telegram-test` and writes the package under
`build/packages/`, which is ignored by git.

## First Offline Test

Run these commands before any live token test:

```text
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-client-self-test
```

A TLS-disabled build should report `unsupported` for HTTPS commands. That is
expected for offline tester packages.

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

The default commands read this file through `PROGDIR:telegram-token.txt`.

If a token is ever exposed in a screenshot, public log or forum post, revoke it
with BotFather using `/revoke` and create a new one.

## Live Tests

Use these with a TLS-enabled build:

```text
telegram-test --telegram-preflight
telegram-test --telegram-getme-default
telegram-test --telegram-get-updates-default
telegram-test --telegram-read-once-state-default telegram-offset.txt
telegram-test --telegram-read-loop-default telegram-offset.txt 5 10
telegram-test --telegram-inbox-default telegram-offset.txt
telegram-test --telegram-inbox-loop-default telegram-offset.txt 5 10
telegram-test --inbox-log-file telegram-inbox.log --telegram-inbox-loop-default telegram-offset.txt 5 10
telegram-test --telegram-session-default telegram-offset.txt telegram-inbox.log telegram-chats.txt
telegram-test --telegram-session-loop-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
telegram-test --telegram-manual-client-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
telegram-test --telegram-client-default
```

For receive tests, send a message to the bot from Telegram before running the
command. The inbox commands are receive-only: they print update id, chat id,
sender name when available, decoded text, message kind and the next saved
offset. Non-text messages currently print placeholders such as `<photo>`,
`<sticker>` or `<document>`.

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

To send a manual response using the 1-based chat list index. Chat index `1` is
the most recently updated chat:

```text
telegram-test --telegram-send-chat-default telegram-chats.txt 1 "Hello from MorphOS"
telegram-test --telegram-reply-default 1 "Hello from MorphOS"
telegram-test --telegram-send-last-default "Hello from MorphOS"
```

The explicit chat-id send command is still available:

```text
telegram-test --telegram-send-default <chat-id> "Hello from MorphOS"
```

## Reporting Results

When reporting MorphOS test results, include:

- MorphOS version;
- whether the build used `ENABLE_TLS=0` or `ENABLE_TLS=1`;
- OpenSSL version if TLS was enabled;
- exact command output from the self-tests or preflight;
- whether the token was a disposable test bot token.

Do not include Telegram tokens or screenshots containing tokens.
