# AROS x86_64 Tester Notes

This target is frozen as a diagnostic/porting lane for AROS x86_64.

Main AROS development continues on AROS i386 until the shell client is usable
end to end. AROS x86_64 will be revisited later with a minimal-runtime port.

Release packages are split by CPU/ABI, not by hosted/native mode. A binary that
matches the AROS x86_64 CPU/ABI should be the same release artifact for hosted,
VM and native AROS x86_64 when the required runtime libraries are present.
Hosted AROS is useful as an extra validation environment, not as a separate
public package category.

The AROS x86_64 path uses a real AROS SDK/toolchain. TLS should follow the
OpenSSL path, not AmiSSL, but HTTPS/live Telegram still requires OpenSSL
development files and runtime libraries that match the SDK/runtime being tested.

## Current Status

### Re-evaluation 2026-06-12

The freeze was re-tested with the current tree (TLS=0, in-tree crypto --
the OpenSSL blocker from May no longer applies) on AROS One x86_64
v0.38 under QEMU:

- Build: clean. `Makefile.aros-x86_64` with `AROS_TOOLCHAIN`
  (`~/amiga-dev/toolchains/aros-x86_64`, gcc 10.5.0) and `AROS_SDK_ROOT`
  (`~/amiga-dev/sdks/aros-one-x86_64/Development`) produces a real
  x86_64 AROS ELF.
- LP64 correctness of the portable layers is proven daily: the host CI
  runs the full MTProto self-test on x86_64 Linux (LP64) on every push.
- Runtime: still down. `--platform-rng-test` dies before reaching the
  program: Software Failure, Error 0x80000003 (illegal address access),
  PC inside `LIBdemon_0_OpenLibrary`, Kickstart ELF Segment -- the
  standard-CRT autoinit's first library open faults. Same wall as May,
  now pinned to an SDK/kickstart ABI mismatch.
- Cheapest future paths, in order: (1) re-link against the Development
  SDK shipped with the *exact* AROS One x64 image being tested, if one
  exists on that disk; (2) the mincrt lane (BebboSSH-style: no autoinit,
  manual opens, minimal stdio shim) -- a real porting project, since the
  client leans on stdio/tmpfile heavily; (3) wait for upstream x86_64
  ABI stabilisation.

### Hosted re-test, same day: the client is x86_64-clean

Rebuilt on the Linux build server against the SDK of its own hosted
AROS x86_64 (deadwood2/AROS, matching toolchain/kickstart pair) and run
inside that hosted runtime via the BebboSSH lane:

- `--platform-rng-test`: full startup, `secure rng: available`.
- `--mtproto-self-test-fast`: encrypted, transport, login, probe,
  crypto and session self-tests all pass.

This pins the native failure precisely: the client and every portable
layer are x86_64-ready today; the crash on AROS One x64 v0.38 is the
SDK/kickstart ABI mismatch of that pairing alone.

Verdict: frozen only as a *public package* until a One-image-matching
SDK exists (check the image's own Development drawer first). The i386
build remains the supported AROS lane; hosted x86_64 is a validated
second oracle for 64-bit correctness.


- Build file present: `Makefile.aros-x86_64`
- TLS backend planned: OpenSSL
- Offline cross-build status: the package helper can now produce a real AROS
  x86_64 ELF and refuses host binaries.
- Runtime status: frozen. The current standard-CRT `telegram-test` crashes
  before `--help` on the AROS x86_64 VM with an illegal address access.
  This points at the standard startup/CRT path, not at Telegram or MTProto
  logic.
- Hosted AROS x86_64 SSH may still be useful for short system commands while
  the hosted runtime is running on the Linux server. It is not a validation
  oracle for this client binary.
- Minimal-runtime diagnostic status: an external BebboSSH-style mincrt probe
  using direct `Write(Output(), ...)` starts on the same AROS x86_64 QEMU target
  and prints output. This confirms that the next x86_64 work should focus on a
  mincrt-compatible client lane instead of further standard-CRT packaging
  tweaks.
- TLS build status: blocked for the validated hosted SDK/runtime pair. OpenSSL
  headers and libraries must come from the same SDK/runtime set being tested;
  a successful cross-link is not enough.
- Live Telegram status: not validated and not planned until the frozen x86_64
  runtime lane is replaced.
- Public package status: diagnostic artifact only:
  `https://github.com/kaffeine1/telegram-amiga/releases/tag/aros-x86_64-offline-prealpha-20260522-22bbd57`
- `--connect-timeout` is accepted by the common CLI, but native AROS currently
  keeps the platform blocking connect path. Do not use unreachable-IP timeout
  tests as an AROS runtime health check yet.

## Frozen Build Sketch

These commands are kept for future porting work only. Do not use this path as a
release gate while x86_64 is frozen.

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

The package helper refuses host binaries. A valid package input should identify
as an AROS x86_64 ELF, for example:

```text
ELF 64-bit LSB relocatable, x86-64, version 1 (AROS Research Operating System)
```

For TLS builds, use only OpenSSL headers and libraries from the same SDK/runtime
set you are validating. A successful cross-link is not enough; run at least
`--help`, `--telegram-client-state-self-test` and `--telegram-tls-status` on the
target before treating the TLS build as usable.

## Minimal Runtime Probe

The repository does not vendor BebboSSH runtime sources. If you have a local
checkout with `aros_mincrt.c` and `aros_startup_flags.c`, build the diagnostic
probe with:

```sh
make -f Makefile.aros-x86_64 probe-mincrt \
  AROS_TOOLCHAIN=/path/to/aros-x86_64-toolchain \
  AROS_SDK_ROOT=/path/to/AROS/Development \
  AROS_X86_64_MINCRT_DIR=/path/to/bebbossh-aros/src
```

Deploy `build/aros-x86_64/minwrite-probe` to AROS x86_64 and run:

```sh
minwrite-probe
```

Expected output:

```text
telegram-amiga mincrt write reached
```

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
