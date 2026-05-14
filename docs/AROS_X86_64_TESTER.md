# AROS x86_64 Tester Notes

This target is planned but not validated yet.

The current tested AROS build is AROS One i386 alt-abiv0 with OpenSSL from the
AROS SDK. AROS x86_64 should follow the same OpenSSL path, not AmiSSL, but it
needs a matching x86_64 AROS SDK/toolchain and target-side validation.

## Current Status

- Build file present: `Makefile.aros-x86_64`
- TLS backend planned: OpenSSL
- Offline cross-build status: validated on a Linux server with the AROS x86_64
  GCC 10.5.0 toolchain and SDK.
- Runtime status: waiting for hosted AROS x86_64 SSH reachability before
  target-side self-tests can be called validated.
- TLS build status: blocked until OpenSSL headers and libraries are available
  in the AROS x86_64 SDK/toolchain.
- Live Telegram status: not validated
- Public package status: not published

## Native Build Sketch

On an AROS x86_64 system with GCC and OpenSSL development files:

```sh
make -f Makefile.aros-x86_64 clean all ENABLE_TLS=1
build/aros-x86_64/telegram-test --telegram-client-self-test
build/aros-x86_64/telegram-test --telegram-tls-status
```

If OpenSSL development files are not available yet, build the offline tester:

```sh
make -f Makefile.aros-x86_64 clean all ENABLE_TLS=0
build/aros-x86_64/telegram-test --telegram-client-self-test
```

## Linux Cross-Build Sketch

With a built AROS x86_64 SDK/toolchain:

```sh
export AROS_ROOT=/path/to/AROS
export AROS_SDK_ROOT=$AROS_ROOT/Development
export AROS_TOOLCHAIN=/path/to/toolchain-core-x86_64
export SDK_GCC_ROOT=$AROS_TOOLCHAIN/lib/gcc/x86_64-aros/10.5.0

make -f Makefile.aros-x86_64 clean all \
  ENABLE_TLS=0 \
  TARGET=build/aros-x86_64/telegram-test
```

`SDK_GCC_ROOT` must point into the GCC toolchain, not into `AROS/Development`,
because the compiler builtin headers live under the toolchain tree.

## First Validation Checklist

Run these before any live Telegram token test:

```sh
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

If TLS is enabled:

```sh
telegram-test --https-test api.telegram.org 443 /
telegram-test --telegram-preflight
```

Live Bot API tests should use only a disposable test bot token until the target
has passed repeated HTTPS tests.
