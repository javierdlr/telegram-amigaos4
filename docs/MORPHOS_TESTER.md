<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# MorphOS Tester Notes

This document describes how to build and run the current MorphOS diagnostic
tester. It includes a pre-alpha manual text client and remains a technical
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

Certificate validation is optional on OpenSSL builds. Use `--tls-verify` with
`--tls-ca-file` or `--tls-ca-path` to request certificate-chain and hostname
verification. Without validation, use this build only for supervised testing
with test bots and disposable tokens.

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

The package also includes helper scripts for the user-facing flow:

```text
Execute RunMorphOSPreflight
Execute RunMorphOSGetMe
Execute RunMorphOSHumanChat
```

Use `Execute`; it is tolerant of ZIP extraction clearing script protection
bits. `RunMorphOSHumanChat` starts the quiet human chat mode without requiring
the user to type long command-line options.

## Experimental Docker Cross-Build

There is also an experimental Docker/pkgsrc cross-build path:

```sh
scripts/docker-morphos-cross-image.sh
scripts/docker-morphos-cross-build.sh
```

It builds `build/morphos-cross/telegram-test` with pkgsrc
`cross/ppc-morphos-gcc`. A Docker-built TLS-disabled binary has passed offline
runtime self-tests on real MorphOS. Keep using the native MorphOS SDK build as
the reference path for TLS-enabled live Bot API tests until the cross-built TLS
path is validated. See `docs/MORPHOS_CROSS_BUILD.md`.

## Common test walkthrough

For the shared Bot-API command walkthrough — offline self-tests, creating a test bot, reading messages, human chat, the manual console and reporting results — see `HOW_TO_TEST.md` and `USER_RUNBOOK.md` (included in this package).
