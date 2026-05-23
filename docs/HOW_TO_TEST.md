<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# How To Test Telegram Amiga

Telegram Amiga is still a pre-alpha technical tester. Use a dedicated test bot
and disposable tokens only. Do not publish tokens or screenshots that show
tokens.

For development regression scope across targets, see `docs/TEST_MATRIX.md`.

## 1. Offline Tests

Run these first. They do not need a Telegram token:

```text
telegram-test --help
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-offset-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-client-state-self-test
telegram-test --telegram-client-self-test
telegram-test --telegram-text-client-self-test
telegram-test --mtproto-self-test-fast
telegram-test --telegram-tls-status
```

Expected result: every self-test prints `ok`. `--telegram-tls-status` should
show whether TLS certificate validation was requested.

`--mtproto-self-test-fast` skips the slow RSA/bigint/SRP samples. Use
`--mtproto-self-test` for the complete MTProto suite, or
`--mtproto-self-test-heavy` to run only the slow MTProto crypto math checks.

## 2. Network And HTTPS Tests

Plain TCP/HTTP tests do not need a token:

```text
telegram-test --net-test example.com 80
telegram-test --http-test example.com 80 /
```

TLS builds can test Telegram HTTPS without sending a token:

```text
telegram-test --https-test api.telegram.org 443 /
telegram-test --telegram-preflight
```

For certificate validation, provide a CA bundle or CA path:

```text
telegram-test --tls-verify --tls-ca-file ca-bundle.crt --https-test api.telegram.org 443 /
telegram-test --tls-verify --tls-ca-file ca-bundle.crt --telegram-preflight
```

Without `--tls-verify`, TLS encrypts traffic but does not validate the
certificate. Keep that mode limited to supervised tests.

Certificate validation also requires a correct system date. If a platform
reports that a certificate is not yet valid or has expired, verify the clock
before debugging TLS code.

## 3. Create A Test Bot

1. Open Telegram on a phone or desktop.
2. Search for `@BotFather`.
3. Send `/newbot`.
4. Choose a display name.
5. Choose a username ending in `bot`.
6. Copy the token into `telegram-token.txt` in the same drawer as
   `telegram-test`.

The token file must contain only the token line. If a token is exposed, revoke
it with BotFather `/revoke`.

## 4. Read Messages

Send a message to the bot from Telegram, then run:

```text
telegram-test --telegram-getme-default
telegram-test --telegram-client-default
```

The default client uses:

```text
telegram-token.txt
telegram-offset.txt
telegram-inbox.log
telegram-chats.txt
```

## 5. Human Chat

For normal human testing, prefer the terse human chat mode:

```text
telegram-test --telegram-human-chat
```

Type normal text to send, press Enter on an empty line to check for replies,
and type `quit` to exit. If no chat is selected yet, send a Telegram message to
the bot and press Enter, or type the Bot API chat id once. This mode does not
redraw a prompt, waits silently when there are no updates, keeps log lines out
of the chat transcript, and still appends `telegram-inbox.log`.

## 6. Manual Console

Use the manual console for diagnostics or explicit command testing:

```text
telegram-test --telegram-client-console
```

Useful commands:

```text
read
list
reply 1 Hello from Telegram Amiga
/send-id 123456789 Hello from Telegram Amiga
chat 1
open 1
last
status
quit
```

`read` polls Telegram and then prints the saved chat list. Use the displayed
chat index with `reply <index> <text>`, or use `chat <index>`, `open <index>`
or a bare numeric index to enter a line-oriented chat mode. In chat mode, type
normal text to send it to the selected chat. Successful chat sends are quiet. If the chat list is still empty but you know the
Telegram Bot API chat id, use `/send-id <chat-id> <text>`; this sends directly
and saves that chat as the selected chat for later `/send <text>` commands.
Use `watch <seconds>` in the top-level console to
auto-read while waiting at the prompt, or `watch off` to disable it. Chat mode
auto-reads every 2 seconds by default while waiting for input; use
`/watch <seconds>` to change the interval, `/watch off` to disable it,
`/read`/`/poll`/`/p` to poll immediately, `/list`/`/chats` to show chats, and
`/back` or `/quit` to leave. The console never sends automatic replies.

## 7. Report Results

Please include:

- platform and version;
- whether the system is real hardware, emulated or virtualized;
- TCP/IP stack;
- TLS library and version, if known;
- whether a CA bundle/path was used;
- full output of the offline tests;
- output of HTTPS tests with and without `--tls-verify`;
- output of `getMe`, read and controlled reply tests, if a token was used;
- crashes, freezes, requesters or unusual delays.

## Platform Notes

- AmigaOS 3.x: see `docs/AMIGAOS3_TESTER.md`.
- MorphOS: see `docs/MORPHOS_TESTER.md`.
- AmigaOS 4.x: see `docs/AMIGAOS4_TESTER.md`.
- AROS: see `docs/AROS_TESTER.md`.
