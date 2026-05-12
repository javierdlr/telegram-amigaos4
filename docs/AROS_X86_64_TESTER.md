# AROS x86_64 Tester Notes

This target is planned but not validated yet.

The current tested AROS build is AROS One i386 alt-abiv0 with OpenSSL from the
AROS SDK. AROS x86_64 should follow the same OpenSSL path, not AmiSSL, but it
needs a matching x86_64 AROS SDK/toolchain and target-side validation.

## Current Status

- Build file present: `Makefile.aros-x86_64`
- TLS backend planned: OpenSSL
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
