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
  an AROS VM with offline self-tests, plain TCP/HTTP diagnostics, HTTPS
  preflight and Telegram `getMe`.
- TLS can be enabled at build time against OpenSSL from the AROS SDK. The first
  live HTTPS and `getMe` tests passed on AROS One i386 alt-abiv0, but
  certificate validation must be explicitly requested with `--tls-verify`.
- AROS One 32-bit has AmiSSL available according to community feedback.
- AROS One 64-bit currently does not have AmiSSL available.
- Live Telegram Bot API `getMe`, read-only polling and controlled
  `sendMessage` have passed on the TLS-enabled AROS One i386 alt-abiv0 build.

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

The default AROS build keeps TLS disabled. A TLS-enabled cross-build can be
attempted with:

```text
make -f Makefile.aros-i386-abiv0 all ENABLE_TLS=1 \
  TOOLCHAIN=/path/to/aros-i386-abiv0 \
  AROS_SDK=/path/to/AROS/Development
```

This links against OpenSSL from the AROS SDK.

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
certificate validation requested: no
```

HTTPS and live Telegram commands need a TLS-enabled build. They have passed
supervised AROS One i386 alt-abiv0 checks. Certificate validation is still
disabled by default, so use unverified TLS only with test bots and disposable
tokens.

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
```

Inside `telegram-client-console`, use `p`/`poll`/`read` to poll, `l`/`list` to
list saved chats, `i`/`last`/`inbox` to show the last inbox log line,
`s`/`status` to show local status, `r`/`send`/`reply <index> <text>` to send a
controlled reply and `q`/`quit` to quit. It does not send replies
automatically.

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
- full output of the offline test commands;
- output of the plain TCP/HTTP diagnostics, if networking is configured;
- whether the build required changing `CC` or other Makefile variables.
