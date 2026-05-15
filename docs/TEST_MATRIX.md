<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# Telegram Amiga Test Matrix

This matrix keeps development moving in parallel across the supported targets
without requiring every target for every small patch. Use disposable Telegram
bot tokens only, and never publish tokens or screenshots containing tokens.

## Patch Levels

Level 0: local portable regression

Run before committing most core changes:

```text
telegram-test --help
telegram-test --telegram-json-self-test
telegram-test --telegram-http-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-offset-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-client-state-self-test
telegram-test --telegram-console-self-test
telegram-test --telegram-client-self-test
telegram-test --telegram-text-client-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-tls-status
```

Level 1: local networking smoke

Run when touching TCP, HTTP, timeout, TLS wrapper, request construction or
platform-facing network behavior:

```text
telegram-test --net-test example.com 80
telegram-test --http-test example.com 80 /
```

Level 2: target offline regression

Run on every target affected by a portable C, CLI, parser, file-state or
console change. This does not need a Telegram token:

```text
telegram-test --help
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-offset-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-client-state-self-test
telegram-test --telegram-console-self-test
telegram-test --telegram-client-self-test
telegram-test --telegram-text-client-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-tls-status
```

Level 3: HTTPS/preflight

Run on TLS-enabled tester builds before live Bot API checks:

```text
telegram-test --https-test api.telegram.org 443 /
telegram-test --telegram-preflight
telegram-test --tls-verify --tls-ca-file ca-bundle.crt --telegram-preflight
```

Certificate validation requires a correct system date.

Level 4: live Bot API smoke

Run only with a disposable or dedicated test bot token:

```text
telegram-test --telegram-getme-default
telegram-test --telegram-client-default
telegram-test --telegram-chats-default
telegram-test --telegram-reply-default 1 "Hello from Telegram Amiga"
```

Before sending by chat index, send a message to the bot and run a receive
command so `telegram-chats.txt` exists. Chat index `1` is the most recently
updated chat.

## Target Cadence

macOS local build with AROS backend:

- Use for Level 0 and Level 1 after most core changes.
- This validates portable logic and host networking, not native AROS runtime.

AmigaOS 3.x:

- Use for Level 2 after state, parser, file and CLI changes.
- Use AmiSSL Level 3/4 for HTTPS-sensitive work.
- Keep `Makefile.amigaos3-gcc` at `-O0` unless the known m68k optimized-build
  miscompile is intentionally revisited.

MorphOS:

- Use for Level 2 after portable workflow changes.
- Use Level 3/4 for OpenSSL/TLS behavior and manual console/send workflows.

AmigaOS 4.x:

- Use QEMU for Level 2 after OS4 build or portable CLI changes.
- Use Level 3/4 when validating AmiSSL and Docker-built binaries.
- Avoid parallel SSH sessions against BebboSSH.

AROS i386:

- Use for AROS networking/TLS regression when available.
- Hosted runtime is preferred for short non-interactive checks.

AROS x86_64:

- Use for Level 2 offline checks for now.
- Treat TLS as blocked until OpenSSL headers and libraries match the validated
  SDK/runtime pair and the resulting binary runs on target.

## Reporting

For any target validation, record:

- target platform/version;
- real hardware, emulated, virtualized or hosted runtime;
- compiler/toolchain;
- TLS library and CA bundle/path, if used;
- command list and result;
- failures, freezes, requesters, SSH/BebboSSH issues or unusual delays.
