<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AROS Tester Notes

Telegram Amiga is not a usable end-user Telegram client yet. On AROS, the
current build is useful for offline parser, JSON, HTTP request-building and
manual-client state tests.

## Current Status

- AROS One 32-bit and 64-bit community builds have compiled the project.
- The AROS backend has an initial BSD-socket TCP implementation using
  `bsdsocket.library`.
- AROS One i386 alt-abiv0 has been cross-built from macOS and smoke-tested in
  an AROS VM with offline self-tests plus plain TCP/HTTP diagnostics.
- TLS still reports unsupported.
- AROS One 32-bit has AmiSSL available according to community feedback.
- AROS One 64-bit currently does not have AmiSSL available.
- Live Telegram Bot API commands need an AROS TCP/TLS backend before they can
  work.

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

Do not mix this with a generic i386 AROS toolchain built for a different ABI:
the resulting binary may not run on AROS One alt-abiv0.

If `make` reports `Clock skew detected`, check the AROS system date/time or
refresh the source timestamps after unpacking the archive.

## Offline Test

```text
telegram-test --help
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-client-self-test
telegram-test --telegram-tls-status
```

Expected TLS status today:

```text
certificate validation: disabled
```

HTTPS and live Telegram commands are expected to fail with `unsupported` until
the TLS platform backend is implemented. Plain TCP/HTTP diagnostics may work on
AROS systems with a compatible BSD socket stack.

Plain network diagnostics:

```text
telegram-test --net-test example.com 80
telegram-test --http-test example.com 80 /
```

These tests do not require a Telegram token.

## Reporting Results

Please report:

- AROS distribution and version;
- 32-bit or 64-bit;
- compiler name and version;
- whether AmiSSL is installed;
- full output of the offline test commands;
- output of the plain TCP/HTTP diagnostics, if networking is configured;
- whether the build required changing `CC` or other Makefile variables.
