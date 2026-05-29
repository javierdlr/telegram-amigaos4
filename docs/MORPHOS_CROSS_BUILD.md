<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# MorphOS Cross Build

This document describes the experimental Docker-based MorphOS cross-build path.
It is separate from the validated native MorphOS SDK build.

The cross toolchain is built from pkgsrc `cross/ppc-morphos-gcc`,
`cross/ppc-morphos-binutils` and `cross/ppc-morphos-sdk`. The default image
uses the `pkgsrc-2026Q1` branch.

## Build The Toolchain Image

Start Docker Desktop or Colima, then run:

```sh
ACCEPT_MORPHOS_SDK_LICENSE=1 \
ACCEPT_LHA_LICENSE=1 \
scripts/docker-morphos-cross-image.sh
```

The image build downloads pkgsrc, bootstraps it under `/opt/pkg`, and builds
`cross/ppc-morphos-gcc`. This is expected to take a while on the first run.
The explicit `ACCEPT_MORPHOS_SDK_LICENSE=1` opt-in is required because pkgsrc
marks `cross/ppc-morphos-sdk` with the `morphos-sdk-license` condition.
The explicit `ACCEPT_LHA_LICENSE=1` opt-in is also required because pkgsrc uses
`archivers/lha` to extract the MorphOS SDK archive.

By default this flow uses an embedded Dockerfile template and does not read a
tracked file from this repository. To use a custom file, pass
`MORPHOS_CROSS_DOCKERFILE=/path/to/Dockerfile`.

Useful overrides:

```sh
ACCEPT_MORPHOS_SDK_LICENSE=1 ACCEPT_LHA_LICENSE=1 MAKE_JOBS=8 scripts/docker-morphos-cross-image.sh
ACCEPT_MORPHOS_SDK_LICENSE=1 ACCEPT_LHA_LICENSE=1 PKGSRC_BRANCH=pkgsrc-2026Q1 scripts/docker-morphos-cross-image.sh
ACCEPT_MORPHOS_SDK_LICENSE=1 ACCEPT_LHA_LICENSE=1 IMAGE=telegram-amiga-morphos-cross:local scripts/docker-morphos-cross-image.sh
```

```sh
MORPHOS_CROSS_DOCKERFILE=/path/to/custom-morphos-cross.Dockerfile \
ACCEPT_MORPHOS_SDK_LICENSE=1 ACCEPT_LHA_LICENSE=1 scripts/docker-morphos-cross-image.sh
```

## Build Telegram Amiga For MorphOS

After the image exists:

```sh
scripts/docker-morphos-cross-build.sh
```

The default output is:

```text
build/morphos-cross/telegram-test
```

The initial target is an offline build with TLS disabled:

```sh
ENABLE_TLS=0 scripts/docker-morphos-cross-build.sh
```

TLS-enabled cross-builds are intentionally not treated as validated yet. To
experiment, pass compatible MorphOS OpenSSL include and library paths:

```sh
ENABLE_TLS=1 \
OPENSSL_CFLAGS="-I/path/to/morphos/openssl/include" \
OPENSSL_LDFLAGS="-L/path/to/morphos/openssl/lib" \
scripts/docker-morphos-cross-build.sh
```

## Validate On MorphOS

Copy `build/morphos-cross/telegram-test` to a MorphOS system. SFTP works well
with the tested BebboSSH setup:

```text
put build/morphos-cross/telegram-test RAM:telegram-test-morphos-cross
```

Then run:

```text
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
telegram-test --telegram-console-self-test
telegram-test --telegram-tls-status
```

Only after those pass should the binary be packaged for testers. For live Bot
API validation, rebuild with a compatible TLS setup and repeat the MorphOS
runtime checks from `docs/MORPHOS_TESTER.md`.

## Status

This path has produced a Docker-built binary that passed the offline runtime
self-tests on real MorphOS. It remains experimental for TLS-enabled/live Bot API
validation. Keep the native `Makefile.morphos` flow as the reference live build
until the cross-built TLS path is validated.
