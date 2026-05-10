<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AmigaOS 4.x Tester Notes

This document describes the current AmigaOS 4.x tester state. It is not a
usable Telegram client yet, but the native AmigaOS 4.x backend can now build
with TCP and optional AmiSSL HTTPS support.

## Current Scope

The AmigaOS 4.x tester can:

- print the platform and data directory;
- run local parser, HTTP builder and Telegram Bot API parser self-tests;
- exercise the manual-client command-line flow;
- connect to Telegram over HTTPS when built with `ENABLE_AMISSL=1`;
- run live `preflight`, `getMe` and read-only polling.

It cannot yet:

- provide a real GUI;
- validate TLS certificates;
- behave as a finished end-user Telegram client.

## Current QEMU Probe

The QEMU target reachable through BebboSSH reported after system updates:

```text
Kickstart 54.57, Workbench 53.21
```

Useful notes from the current setup:

- SSH/BebboSSH is reachable through a forwarded localhost port.
- `System:`, `Work:` and `RAM:` are mounted.
- AmigaOS 4 SDK 54.16 is installed in `Work:SDK`.
- GCC 11.2.0 is available through `SDK:S/sdk-startup`.
- The SDK startup is persisted from `S:User-Startup`.
- AmiSSL runtime was updated and live TLS was verified with:
  `amisslmaster.library 5.25 (12-Oct-2025)`.

The SDK packages needed for native builds are:

- base SDK;
- `execsg_sdk-54.31.lha`;
- `gcc-noarch.lha`;
- one GCC binary package, currently tested with `gcc-11.2.0-bin.lha`;
- `newlib-53.80.lha`;
- `AmiSSL-5.3.lha` for AmiSSL headers and `libamisslstubs.a`.

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

To build the AmiSSL HTTPS tester:

```text
make -f Makefile.amigaos4 all ENABLE_AMISSL=1
```

If `make` is not installed yet but `gcc` is available, use the included
AmigaDOS helper from the project drawer:

```text
Execute scripts/BuildAmigaOS4Offline
```

The helper compiles the current offline tester and runs the parser/inbox/send
self-tests.

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

For the AmiSSL build, also run:

```text
telegram-test --telegram-preflight
telegram-test --telegram-getme RAM:telegram-token.txt
telegram-test --telegram-read-loop RAM:telegram-token.txt RAM:telegram-offset-os4.txt 0 1
```

The token file must contain only the BotFather token line. Do not include it in
logs or screenshots.

## Confirmed Live Results

On the QEMU AmigaOS 4.x target, the AmiSSL build has been verified with:

- `--telegram-preflight`: HTTP 302 and ok;
- `--telegram-getme`: HTTP 200 and ok for `KaffoAmigaBot`;
- `--telegram-read-loop`: one read-only iteration, rc 0, no update available.

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
- exact output from the offline or live tester commands.

Do not include Telegram tokens or screenshots containing tokens.
