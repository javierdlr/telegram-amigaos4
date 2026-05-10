<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AmigaOS 4.x Tester Notes

This document describes the current AmigaOS 4.x preparation state. It is not a
usable Telegram client yet. The AmigaOS 4.x backend is currently an offline
tester target: startup, command-line parsing and core self-tests are useful, but
TCP/TLS still report unsupported.

## Current Scope

The AmigaOS 4.x tester can:

- print the platform and data directory;
- run local parser, HTTP builder and Telegram Bot API parser self-tests;
- exercise the manual-client command-line flow without live networking.

It cannot yet:

- connect to Telegram;
- use AmiSSL for HTTPS;
- run live `getMe`, `getUpdates` or `sendMessage`.

## Current QEMU Probe

The first QEMU target reachable through BebboSSH reported:

```text
Kickstart 54.57, Workbench 53.14
```

Useful notes from the probe:

- SSH/BebboSSH is reachable through a forwarded localhost port.
- `System:`, `Work:` and `RAM:` are mounted.
- `gcc`, `make`, `wget` and `unzip` were not in the target PATH.
- The installed AmiSSL master was old:
  `amisslmaster.library 3.7 (2-Apr-2006) OS4 version`.

Live Telegram HTTPS will need a modern AmiSSL 5 setup or another supported TLS
backend before it can be tested.

## Build Options

From a Mac or Unix-like host with a PPC AmigaOS 4 cross-compiler:

```sh
make -f Makefile.amigaos4 clean all \
  CC=ppc-amigaos-gcc \
  TARGET=build/amigaos4/telegram-test
```

From native AmigaOS 4.x, first install a working SDK/GCC environment and ensure
the compiler and make are in the shell path. Then build from the project drawer:

```text
make -f Makefile.amigaos4 all
```

If `make` is not installed yet but `gcc` is available, use the included
AmigaDOS helper from the project drawer:

```text
Execute scripts/BuildAmigaOS4Offline
```

The helper compiles the current offline tester and runs the parser/inbox/send
self-tests. The current source build is intended for offline self-tests only.

## First Offline Test

Run these commands before any live token or networking work:

```text
telegram-test --help
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
```

Network or HTTPS commands are expected to fail with `unsupported` until the
AmigaOS 4.x platform backend is implemented.

## Packaging From The Mac

After building or copying an AmigaOS 4.x binary to:

```text
build/amigaos4/telegram-test
```

create a local tester package:

```sh
scripts/package-amigaos4-tester.sh
```

The package is written under `build/packages/`, which is ignored by git. It does
not include Telegram tokens or AmiSSL runtime files.

## Reporting Results

When reporting AmigaOS 4.x results, include:

- AmigaOS 4.x version;
- whether the build was native or cross-compiled;
- compiler version and SDK source;
- AmiSSL version if TLS work is being attempted;
- exact output from the offline self-tests.

Do not include Telegram tokens or screenshots containing tokens.
