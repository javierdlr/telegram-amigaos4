<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AROS Tester Notes

Telegram Amiga now includes a pre-alpha manual text client. On AROS, the
current tester can exercise the same Bot API based diagnostic flow used on the
other targets: offline parser/state tests, TCP/HTTP diagnostics, HTTPS,
`getMe`, read-only polling, saved-chat replies and controlled `sendMessage`.

## Current Status

- AROS One 32-bit and 64-bit community builds have compiled the project.
- The AROS backend has an initial BSD-socket TCP implementation using
  `bsdsocket.library`.
- AROS One i386 alt-abiv0 has been cross-built from macOS and smoke-tested in
  an AROS VM with offline self-tests, TCP/HTTP diagnostics, HTTPS, preflight,
  Telegram `getMe`, read-only polling and controlled `sendMessage`.
- TLS can be enabled at build time against OpenSSL from the AROS SDK.
  Certificate validation is available and must be explicitly requested with
  `--tls-verify`.
- AROS One 32-bit has AmiSSL available according to community feedback.
- AROS One 64-bit currently does not have AmiSSL available.
- AROS x86_64 has an experimental `Makefile.aros-x86_64` and should follow the
  OpenSSL path, but it has not been validated yet.
- Live Telegram Bot API `getMe`, read-only polling and controlled
  `sendMessage` have passed on the TLS-enabled AROS One i386 alt-abiv0 build.
- TLS certificate validation with an explicit CA bundle has passed against
  `api.telegram.org` on the AROS One i386 alt-abiv0 VM.

## Build

On native AROS:

```text
make -f Makefile.aros all
```

`Makefile.aros` defaults to native `gcc`. Cross-builds can still override the
compiler:

```text
make -f Makefile.aros CC=i386-aros-gcc all
```

For AROS One i386 alt-abiv0 cross-builds from a host system, use the dedicated
helper and point it at the matching toolchain and SDK:

```text
make -f Makefile.aros-i386-abiv0 all \
  TOOLCHAIN=/path/to/aros-i386-abiv0 \
  AROS_SDK=/path/to/AROS/Development
```

If you need to override only the compiler executable, use `AROS_CC=...`.
If the GCC builtin headers are not under `$(AROS_SDK)/lib/gcc/i386-aros/6.5.0`,
also set `SDK_GCC_ROOT=/path/to/lib/gcc/i386-aros/<version>`.

Do not mix this with a generic i386 AROS toolchain built for a different ABI:
the resulting binary may not run on AROS One alt-abiv0.

The default AROS build keeps TLS disabled. A TLS-enabled cross-build can be
attempted with:

```text
make -f Makefile.aros-i386-abiv0 all ENABLE_TLS=1 \
  TOOLCHAIN=/path/to/aros-i386-abiv0 \
  AROS_SDK=/path/to/AROS/Development
```

This links against OpenSSL from the AROS SDK.

For hosted AROS i386 SDK/toolchains built from current AROS sources, the same
Makefile can be pointed at the hosted SDK:

```text
make -f Makefile.aros-i386-abiv0 all ENABLE_TLS=0 \
  TOOLCHAIN=/path/to/toolchain-core-i386 \
  AROS_CC=/path/to/toolchain-core-i386/i386-aros-gcc \
  AROS_SDK=/path/to/core-linux-i386-host-i386-d/bin/linux-i386/AROS/Development \
  SDK_GCC_ROOT=/path/to/toolchain-core-i386/lib/gcc/i386-aros/10.5.0
```

This hosted i386 path is currently useful for offline build smoke tests. It is
not yet a replacement for the AROS One i386 alt-abiv0 live Telegram validation.
With BebboSSHd AROS commit `eae8a99` or newer, the hosted i386 runtime has
passed the full offline self-test list below over short non-interactive SSH
commands.

For initial AROS x86_64 experiments, use:

```text
make -f Makefile.aros-x86_64 all ENABLE_TLS=1
```

That target is not validated yet. See `docs/AROS_X86_64_TESTER.md`.

If `make` reports `Clock skew detected`, check the AROS system date/time or
refresh the source timestamps after unpacking the archive.

## Offline Test

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
telegram-test --telegram-tls-status
```

Expected TLS status without validation flags:

```text
certificate validation requested: no
```

HTTPS and live Telegram commands need a TLS-enabled build. They have passed
supervised AROS One i386 alt-abiv0 checks. Certificate validation is disabled
unless `--tls-verify` is supplied, so use unverified TLS only with test bots
and disposable tokens.

Plain network diagnostics:

```text
telegram-test --net-test example.com 80
telegram-test --http-test example.com 80 /
```

These tests do not require a Telegram token.

TLS diagnostics, no Telegram token required:

```text
telegram-test --https-test api.telegram.org 443 /
telegram-test --telegram-preflight
```

TLS diagnostics with certificate validation:

```text
telegram-test --tls-verify --tls-ca-file ca-bundle.crt --https-test api.telegram.org 443 /
telegram-test --tls-verify --tls-ca-file ca-bundle.crt --telegram-preflight
```

On the current AROS One test VM, a YAM CA bundle from the AROS One DVD passed
this check after being copied to `DH0:TGTEST/ca-bundle.crt`.

If DNS resolution fails immediately after boot or after several short network
tests, run a simple network diagnostic such as `--net-test api.telegram.org 443`
and retry the HTTPS command.

Live Bot API check, after creating `telegram-token.txt` in the same drawer:

```text
telegram-test --data-dir PROGDIR: --telegram-getme-default
telegram-test --data-dir PROGDIR: --telegram-read-loop-default telegram-offset.txt 5 10
telegram-test --data-dir PROGDIR: --telegram-client-default
telegram-test --data-dir PROGDIR: --telegram-client-console
telegram-test --data-dir PROGDIR: --telegram-human-chat
```

Inside `telegram-client-console`, use `/read` or `/refresh` to poll, `/chats`
to list saved chats, `/last` to show the last inbox line, `/status` to show
local status, `/open <index>` or a bare numeric index to enter a
line-oriented chat, `/send <text>` to send to the selected chat,
`/send-id <chat-id> <text>` to send directly when the chat list is empty and
`/quit` to quit. The selected chat is persisted in
`telegram-selected-chat.txt`.
Inside chat mode, type normal text to send. Use `/watch <seconds>` in the
top-level prompt or chat mode to auto-read while waiting, or `/watch off` to
disable it. It does not send replies automatically.

For a terse human chat, use
`telegram-test --data-dir PROGDIR: --telegram-human-chat`. Type normal text to
send, press Enter on an empty line to check for replies, and type `quit` to
exit. If no chat is selected yet, send a Telegram message to the bot and press
Enter, or type the Bot API chat id once. This mode keeps log lines out of the chat
transcript, but still appends `telegram-inbox.log`.

Once a chat has been saved by polling a message from the bot, send by saved
chat index with:

```text
telegram-test --data-dir PROGDIR: --telegram-chats-default
telegram-test --data-dir PROGDIR: --telegram-reply-default 1 "Hello from AROS"
telegram-test --data-dir PROGDIR: --telegram-send-last-default "Hello from AROS"
```

## Reporting Results

Please report:

- AROS distribution and version;
- 32-bit or 64-bit;
- compiler name and version;
- whether AmiSSL is installed;
- whether OpenSSL/TLS was enabled;
- whether `--tls-verify` was tested and which CA bundle/path was used;
- full output of the offline test commands;
- output of the plain TCP/HTTP diagnostics, if networking is configured;
- output of HTTPS, `getMe`, read-only polling and controlled reply/send tests,
  if a disposable token was used;
- whether the build required changing `CC` or other Makefile variables.

Do not include Telegram tokens or screenshots containing tokens.
