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

- AROS One 32-bit is the active AROS development target.
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
- AROS x86_64 is frozen as a diagnostic/porting lane. The current
  standard-CRT x86_64 binary crashes before `--help` on the AROS x86_64 VM.
  Continue main client work on AROS i386; revisit x86_64 later through a
  minimal-runtime port.
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

The AROS x86_64 build path is currently diagnostic only. Use it only when
working on the frozen x86_64 porting lane:

```text
make -f Makefile.aros-x86_64 all ENABLE_TLS=0 \
  AROS_TOOLCHAIN=/path/to/aros-x86_64-toolchain \
  AROS_SDK_ROOT=/path/to/AROS/Development
```

The resulting file must be an AROS x86_64 ELF, not a host executable, but the
current standard-CRT output is not runtime-valid. See
`docs/AROS_X86_64_TESTER.md`.

If `make` reports `Clock skew detected`, check the AROS system date/time or
refresh the source timestamps after unpacking the archive.

## Common test walkthrough

For the shared Bot-API command walkthrough — offline self-tests, creating a test bot, reading messages, human chat, the manual console and reporting results — see `HOW_TO_TEST.md` and `USER_RUNBOOK.md` (included in this package). On AROS, prefix commands with `--data-dir PROGDIR:` as shown below where required.

## AROS Notes

Expected TLS status without validation flags:

```text
certificate validation requested: no
```

HTTPS and live Telegram commands need a TLS-enabled build. They have passed
supervised AROS One i386 alt-abiv0 checks. Certificate validation is disabled
unless `--tls-verify` is supplied, so use unverified TLS only with test bots
and disposable tokens.

TLS-enabled tester packages also include helper scripts for the user-facing
flow:

```text
Execute RunAROSPreflight
Execute RunAROSGetMe
Execute RunAROSHumanChat
```

Use `Execute`; it is tolerant of ZIP extraction clearing script protection
bits. The helpers pass `--data-dir PROGDIR:` so token and state files stay next
to `telegram-test`.

On the current AROS One test VM, a YAM CA bundle from the AROS One DVD passed
the certificate-validation check after being copied to
`DH0:TGTEST/ca-bundle.crt`.

If DNS resolution fails immediately after boot or after several short network
tests, run a simple network diagnostic such as `--net-test api.telegram.org 443`
and retry the HTTPS command.

On AROS, the default-flow Bot API commands take a `--data-dir PROGDIR:` prefix
so token and state files stay next to `telegram-test`:

```text
telegram-test --data-dir PROGDIR: --telegram-getme-default
telegram-test --data-dir PROGDIR: --telegram-read-loop-default telegram-offset.txt 5 10
telegram-test --data-dir PROGDIR: --telegram-client-default
telegram-test --data-dir PROGDIR: --telegram-client-console
telegram-test --data-dir PROGDIR: --telegram-human-chat
```

Once a chat has been saved by polling a message from the bot, send by saved
chat index with:

```text
telegram-test --data-dir PROGDIR: --telegram-chats-default
telegram-test --data-dir PROGDIR: --telegram-reply-default 1 "Hello from AROS"
telegram-test --data-dir PROGDIR: --telegram-send-last-default "Hello from AROS"
```
