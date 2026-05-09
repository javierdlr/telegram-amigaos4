<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# Telegram Amiga

License: MIT

Telegram Amiga is a non-commercial community project, created as a gift to the
Amiga community and as an exploration of what a modern Telegram client for
Amiga-like systems could become.

Current status: early technical bootstrap, not yet a usable Telegram client.

## Goal

Build, one step at a time, a cross-platform Telegram client for:

- AmigaOS 3.x
- MorphOS
- AmigaOS 4.x
- AROS

The project favors portable C code, platform-specific backends and incremental
testing on real hardware or real target systems.

## AI-Assisted Development

This project is also developed with the help of AI agents, used as assisted
development tools for code analysis, incremental implementation, refactoring,
documentation and test preparation.

The goal is not to replace the experience of the Amiga community. The goal is to
show that AI agents can become a practical support even for historical or niche
platforms, where time, documentation, hardware and toolchains are often precious
resources.

The workflow uses a Mac as the main development machine. Real or emulated
Amiga-like systems are made reachable on the local network through SSH, for
example with BebboSSH on AmigaOS or MorphOS. This lets the agent prepare code on
the Mac, copy it to the target system, run builds and tests, and report concrete
results instead of working only in theory.

This repository is also a small public experiment: can collaboration between
people, real Amiga hardware and AI agents help produce useful new software for
the community?

## Structure

Minimal bootstrap:

- `core/`: portable logic
- `docs/`: target notes and tester instructions
- `include/`: internal public interfaces
- `platforms/*/`: platform-specific adapters
- `scripts/`: local helper scripts for builds and packaging
- `src/main.c`: thin entry point

Initial core modules:

- `tg_config`: minimal command-line argument parsing
- `tg_bot`: Bot API orchestration helpers
- `tg_log`: portable logging delegated to the platform layer
- `tg_http`: minimal HTTP/1.0 GET/POST over `tg_net`, plus response parsing
- `tg_json`: minimal top-level JSON field lookup plus JSON string escape decoding
- `tg_net`: portable TCP API with MorphOS and initial AmigaOS 3.x backends
- `tg_telegram`: Telegram API response envelope parsing
- `tg_tls`/`tg_https`: minimal TLS/HTTPS with MorphOS OpenSSL and optional
  AmigaOS 3.x AmiSSL backends
- Bot API `getMe`, `getUpdates` and `sendMessage` helpers; `getUpdates` can
  extract the first update id, chat id and text from the returned array.
- A one-shot echo command can read one update, print the next offset and send an
  `Echo: ...` reply when the update contains text. JSON string escapes are
  decoded before displaying or echoing text.
- A bounded echo loop can repeat the stateful one-shot flow with caller-chosen
  polling seconds and a maximum iteration count.
- Default-token command variants can load `telegram-token.txt` from the active
  data directory, or from a path supplied with `--token-file`.
- `--telegram-preflight` checks the default token path and verifies HTTPS
  reachability to Telegram without sending the token.

TLS note: the current MorphOS and AmigaOS 3.x backends use OpenSSL/AmiSSL with
SNI, but certificate validation is not enabled yet. This is enough for
connectivity tests, not yet for secure use.

Initial targets:

- MorphOS: active and verified
- AmigaOS 3.x: TCP/HTTP verified on real hardware; optional AmiSSL HTTPS,
  Telegram `getMe` and `sendMessage` verified on Vampire/AmiKit with AmiSSL v5
- AmigaOS 4.x: stub ready, toolchain to install
- AROS: stub ready, toolchain to install

Build on MorphOS:

```text
System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos run
```

Remote build from the Mac:

```sh
ssh user@<morphos-host> 'System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos run'
```

Cross-build for AmigaOS 3.x from the Mac:

```sh
PATH=/path/to/m68k-amigaos-toolchain/bin:$PATH \
make -f Makefile.amigaos3-gcc all
```

The AmigaOS 3.x GCC helper currently uses an ixemul-based build and `-O0`.
This is intentional: the tested m68k GCC runtime is incomplete for default
linking, and an optimized `-O2` build miscompiled one JSON escaping self-test on
the tested setup.

Optional AmigaOS 3.x HTTPS/AmiSSL build:

```sh
PATH=/path/to/m68k-amigaos-toolchain/bin:$PATH \
make -f Makefile.amigaos3-gcc \
  ENABLE_AMISSL=1 \
  AMISSL_SDK=/path/to/AmiSSL/Developer \
  TARGET=build/amigaos3/telegram-test-amissl
```

The AmiSSL build needs AmiSSL v5 installed on the target system, with
`AmiSSL:` assigned to the AmiSSL install directory and `LIBS:` extended with
`AmiSSL:Libs`. On AmigaOS 3.x/Vampire, choose the AmiSSL `68020/030/040/080`
library variant. If an older `amisslmaster.library` is already resident after
an upgrade, reboot or run `Avail FLUSH` before testing so `OpenLibrary()` sees
the newer master.

AmigaOS 3.x tester package:

```sh
scripts/package-amigaos3-tester.sh
```

The script builds the AmiSSL-enabled 68k tester and creates a local package
under `build/packages/`. See `docs/AMIGAOS3_TESTER.md` for target-side
requirements, commands and reporting notes. The package does not include
Telegram tokens or AmiSSL runtime files.

Flow Studio on MorphOS:

- `default.sprj` is the auto-loadable Flow Studio project.
- `telegram-amiga.xprj` can be opened manually from the project requester.
- Open a source file from `Work:Dev/telegram-amiga` or start Flow Studio with
  `PROJECT=Work:Dev/telegram-amiga/default.sprj`.
- The build uses `System:Development/gg/bin/make -f Makefile.morphos all`.
- If opening only the project shows just `Build Rules`, run
  `execute Work:Dev/telegram-amiga/OpenTelegramAmiga.flow`: it opens the project
  together with the main source files, so the Project Lister has an active C
  file from which it can populate Source/Header/Build.
- `telegram-amiga.files` contains the list of the main project files.

Current options:

```text
-h, --help            Show help
-v, --verbose         Enable debug logging
-q, --quiet           Show warnings and errors only
    --data-dir <path> Set application data directory
    --token-file <path>
                      Override default Telegram token file
    --net-test <host> <port>
                      Test DNS resolution and TCP connection
    --http-test <host> <port> <path>
                      Test TCP connect/send/recv with HTTP/1.0
    --http-post-self-test
                      Run built-in HTTP POST request builder sample
    --https-test <host> <port> <path>
                      Test TLS connect/send/recv with HTTP/1.0
    --json-test <json> <field>
                      Test top-level JSON field lookup
    --telegram-json-test <json>
                      Test Telegram API response parsing
    --telegram-json-self-test
                      Run built-in Telegram JSON parser samples
    --telegram-path-test <token> <method>
                      Test Telegram Bot API path construction
    --telegram-http-self-test
                      Run built-in HTTP-to-Telegram parser samples
    --telegram-token-file-path-test <file> <method>
                      Load token file and test Bot API path construction
    --telegram-default-token-file-path-test <method>
                      Load default token file and test Bot API path construction
    --telegram-preflight
                      Check token path and Telegram HTTPS reachability
    --telegram-getme-self-test
                      Run built-in Bot API getMe parser sample
    --telegram-getme <file>
                      Call Telegram getMe with token loaded from file
    --telegram-getme-default
                      Call Telegram getMe with default token file
    --telegram-get-updates-self-test
                      Run built-in Bot API getUpdates parser sample
    --telegram-get-updates <file> [offset]
                      Call Telegram getUpdates with optional offset
    --telegram-get-updates-default [offset]
                      Call Telegram getUpdates with default token file
    --telegram-echo-once-self-test
                      Run built-in one-shot echo flow sample
    --telegram-echo-once <file> [offset]
                      Read one update and echo its text back
    --telegram-echo-once-default [offset]
                      Echo one update using the default token file
    --telegram-echo-once-state <file> <offset-file>
                      Echo one update using a persistent offset file
    --telegram-echo-once-state-default <offset-file>
                      Stateful echo one update with default token file
    --telegram-echo-loop <file> <offset-file> <poll-seconds> <max-iterations>
                      Run bounded stateful echo polling
    --telegram-echo-loop-default <offset-file> <poll-seconds> <max-iterations>
                      Run bounded stateful echo polling with default token file
    --telegram-send-message-self-test
                      Run built-in Bot API sendMessage parser sample
    --telegram-send-message <file> <chat-id> <text>
                      Send a Telegram message with token loaded from file
    --telegram-send-message-default <chat-id> <text>
                      Send a Telegram message with default token file
```

`getUpdates` prints the raw Telegram result and, when present, a minimal first
update summary:

```text
telegram update id: 64052626
telegram update chat id: 148319454
telegram update text: /start
```

`telegram-echo-once` is intentionally not a permanent loop. Run it again with
the printed `telegram next offset` value to avoid processing the same update
twice. Do not use arbitrary large offsets as a substitute for state; keep and
reuse the actual next offset printed by the program.

For repeated one-shot runs, prefer `telegram-echo-once-state`. It reads the
offset from a caller-provided text file and saves the next offset after a
successful send or after deliberately skipping a non-text update.

Use fake tokens for path tests and examples. Real Bot API tokens should not be
committed, pasted into public issues or shared in logs.

Commands ending in `-default` load the token from `telegram-token.txt` inside
the active data directory. With the default AmigaOS-style data directory this is
`PROGDIR:telegram-token.txt`; on Unix-like paths a slash is inserted when
needed. `--token-file <path>` overrides that computed path.

`telegram-preflight` is useful before giving a tester build to someone else. It
prints the resolved token file path, reports whether the file is present, then
performs an HTTPS request to `https://api.telegram.org/`. It does not call a Bot
API method and does not send or print the token.

`telegram-echo-loop` is deliberately bounded rather than daemon-style. It
reuses the same persistent offset file as `telegram-echo-once-state`, sleeps
between iterations when `poll-seconds` is greater than zero, and stops after
`max-iterations` or on the first error. The accepted limits are
`poll-seconds <= 3600` and `1 <= max-iterations <= 10000`.

Note: through BebboSSH, the remote shell does not always preserve the AmigaDOS
PATH, so the Makefile uses absolute paths to the MorphOS SDK.

Planned Makefiles:

- `Makefile.morphos`
- `Makefile.amigaos3`
- `Makefile.amigaos3-gcc`
- `Makefile.amigaos4`
- `Makefile.aros`

## License

This project is distributed under the MIT License. See `LICENSE` for the full
text.
