<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AmigaOS 4.x Tester Notes

This document describes how to build and run the current AmigaOS 4.x
diagnostic tester. It includes a pre-alpha manual text client and remains a
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

AmiSSL certificate validation is opt-in with `--tls-verify` and either a CA
bundle supplied through `--tls-ca-file` or a usable AmiSSL/OpenSSL default
trust store. This has passed a supervised HTTPS/preflight smoke test on the
project AmigaOS 4.x QEMU setup with an explicit CA bundle. The system date must
be correct before certificate validation can succeed.

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

## Cross-Build With Docker

The project can be cross-built from macOS/Linux with the
`walkero/amigagccondocker:os4-gcc11` container. The local helper workspace used
for supervised testing is outside this repository:

```text
/Users/kaffeine/amiga-dev/projects/os4-cross
```

The container has AmiSSL headers and `libamisslauto.a`, but not
`libamisslstubs.a`. Native AmigaOS 4.x builds keep using the default
`AMISSL_LIB=amisslstubs`; Docker builds should override it:

```sh
cd /Users/kaffeine/amiga-dev/projects/os4-cross
colima start
rm -rf work/telegram-amiga
rsync -a --exclude .git --exclude build \
  /Users/kaffeine/amiga-dev/projects/telegram-amiga/ work/telegram-amiga/

./bin/os4-run sh -lc 'cd telegram-amiga &&
  make -f Makefile.amigaos4 clean all \
    ENABLE_AMISSL=1 \
    AMISSL_LIB=amisslauto \
    AMISSL_LIBDIR=/opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib/lib \
    TARGET=build/os4-cross-amissl/telegram-test'
```

If the Docker compiler exits with an internal compiler error at `-O2`, rerun
the same build with `CFLAGS="-Wall -Wextra -O0 -Iinclude"`. This is a
toolchain stability fallback; use it to keep validation moving when the crash is
inside `cc1` and not tied to a project diagnostic.

The resulting binary is an ELF PowerPC AmigaOS 4.x executable. The Docker-built
AmiSSL binary has passed these QEMU AmigaOS 4.x checks:

```text
RAM:telegram-test-os4-cross --telegram-console-self-test
RAM:telegram-test-os4-cross --telegram-client-state-self-test
RAM:telegram-test-os4-cross --telegram-client-self-test
RAM:telegram-test-os4-cross --telegram-text-client-self-test
RAM:telegram-test-os4-cross --telegram-tls-status
```

Reboot the QEMU target before validation if previous SSH-driven Telegram tests
timed out or left stale output in BebboSSHd.

If `make` is not available but `gcc` is available, use the included AmigaDOS
helper from the project drawer:

```text
Execute scripts/BuildAmigaOS4Offline
```

The helper compiles the current offline tester and runs the parser/inbox/send
self-tests.

If AmiSSL SDK stubs are installed, use the AmiSSL helper:

```text
Execute scripts/BuildAmigaOS4AmiSSL
```

It builds `build/telegram-test-amissl` and runs the client/TLS status checks.

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

The tester package also includes helper scripts for the user-facing flow:

```text
Execute RunAmigaOS4Preflight
Execute RunAmigaOS4GetMe
Execute RunAmigaOS4HumanChat
```

Use `Execute`; it is tolerant of ZIP extraction clearing script protection
bits. `RunAmigaOS4HumanChat` starts the quiet human chat mode without requiring
the user to type long command-line options.

## Common test walkthrough

For the shared Bot-API command walkthrough — offline self-tests, creating a test bot, reading messages, human chat, the manual console and reporting results — see `HOW_TO_TEST.md` and `USER_RUNBOOK.md` (included in this package).

## AmigaOS 4.x Certificate Date Note

If certificate validation fails with a message such as `certificate is not yet
valid`, check the AmigaOS 4.x system date first. The QEMU test target does not
always restore the correct clock after reboot.

## Confirmed Live Results

On the QEMU AmigaOS 4.x target, the AmiSSL build has been verified with:

- `--telegram-preflight`: HTTP 302 and ok;
- `--telegram-getme`: HTTP 200 and ok for `KaffoAmigaBot`;
- `--telegram-read-loop`: one read-only iteration, rc 0, no update available.
