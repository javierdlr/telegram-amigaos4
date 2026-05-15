# AROS x86_64 Tester Notes

This target is an offline pre-alpha tester.

The current AROS x86_64 path uses the AROS SDK/toolchain on a Linux server and
hosted AROS x86_64 for short non-interactive runtime checks. TLS should follow
the OpenSSL path, not AmiSSL, but HTTPS/live Telegram still requires OpenSSL
development files that match the validated SDK and hosted runtime.

## Current Status

- Build file present: `Makefile.aros-x86_64`
- TLS backend planned: OpenSSL
- Offline cross-build status: validated on a Linux server with the AROS x86_64
  GCC 10.5.0 toolchain and SDK.
- Runtime status: hosted AROS x86_64 SSH is usable for short non-interactive
  commands while the hosted runtime is running on the Linux server.
  `10.255.222.2:2222` is a TAP-internal endpoint, not a permanent public
  service.
- Offline target-side status: core self-tests have passed over hosted AROS
  x86_64 SSH.
- TLS build status: blocked for the validated hosted SDK/runtime pair. OpenSSL
  headers and libraries must come from the same SDK/runtime set being tested;
  a successful cross-link is not enough.
- Live Telegram status: not validated
- Public package status: published as an offline pre-alpha tester:
  `https://github.com/kaffeine1/telegram-amiga/releases/tag/aros-x86_64-offline-prealpha-20260514-fadc278`
- `--connect-timeout` is accepted by the common CLI, but native AROS currently
  keeps the platform blocking connect path. Do not use unreachable-IP timeout
  tests as an AROS runtime health check yet.

## Native Build Sketch

On an AROS x86_64 system with GCC and OpenSSL development files:

```sh
make -f Makefile.aros-x86_64 clean all ENABLE_TLS=1
build/aros-x86_64/telegram-test --telegram-client-state-self-test
build/aros-x86_64/telegram-test --telegram-client-self-test
build/aros-x86_64/telegram-test --telegram-text-client-self-test
build/aros-x86_64/telegram-test --telegram-tls-status
```

If OpenSSL development files are not available yet, build the offline tester:

```sh
make -f Makefile.aros-x86_64 clean all ENABLE_TLS=0
build/aros-x86_64/telegram-test --telegram-client-state-self-test
build/aros-x86_64/telegram-test --telegram-client-self-test
build/aros-x86_64/telegram-test --telegram-text-client-self-test
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

For TLS builds, use only OpenSSL headers and libraries from the same SDK/runtime
set you are validating. A successful cross-link is not enough; run at least
`--help`, `--telegram-client-state-self-test` and `--telegram-tls-status` on the
target before treating the TLS build as usable.

## First Validation Checklist

Run these before any live Telegram token test:

```sh
telegram-test --help
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
telegram-test --telegram-tls-status
```

If TLS is enabled:

```sh
telegram-test --https-test api.telegram.org 443 /
telegram-test --telegram-preflight
```

Live Bot API tests should use only a disposable test bot token until the target
has passed repeated HTTPS tests.

## Hosted Runtime Access

When the hosted AROS x86_64 runtime is running on the Linux server, short
non-interactive commands can be executed from that server:

```sh
sshpass -p test ssh -p 2222 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -o PreferredAuthentications=password \
  -o PubkeyAuthentication=no \
  test@10.255.222.2 "C:Version"
```

`10.255.222.2:2222` is reachable only from inside the server while the TAP
runtime is active. From another machine, use an SSH tunnel to the server first,
then connect to the forwarded local port.

Current limitations:

- Use short non-interactive commands only.
- Avoid remote redirection and pipes.
- Avoid `RAM:` for persistent tests.
- Interactive console mode is not validated on this SSH path.
- Unreachable-host network timeout tests are not validated on native AROS yet.
- BebboSSHd x64 v0.3.1 or newer is required for larger command stack. Older
  builds could run `telegram-test` out of stack during heavier self-tests.
