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
- The AROS backend currently reports networking/TLS as unsupported.
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
the platform backend is implemented.

## Reporting Results

Please report:

- AROS distribution and version;
- 32-bit or 64-bit;
- compiler name and version;
- whether AmiSSL is installed;
- full output of the offline test commands;
- whether the build required changing `CC` or other Makefile variables.
