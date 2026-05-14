<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# Telegram Amiga

Experimental Telegram Bot API client core for Amiga-like systems.

**Not a full Telegram Desktop/mobile replacement.**

License: MIT

Telegram Amiga is a non-commercial community project, created as a gift to the
Amiga community and as an exploration of what a lightweight Telegram-related
client/tool for Amiga-like systems could become.

Current status: early technical bootstrap, not yet a usable end-user Telegram
client. The current implementation uses the Telegram Bot API as a practical
bootstrap layer for networking, TLS, JSON parsing, polling and text-message
experiments.

## What it is / What it is not

What it is:

- a portable C networking/TLS/Telegram Bot API experiment
- a diagnostic/test client for Amiga-like systems
- a possible base for lightweight text messaging tools
- an incremental way to verify real MorphOS, AmigaOS 3.x, AmigaOS 4.x and AROS
  support

What it is not:

- not a full Telegram user client yet
- not an MTProto implementation
- not a replacement for Telegram Desktop or Telegram mobile apps
- no login with a personal Telegram account
- no automatic heavy media support

The first realistic milestone is reliable lightweight text messaging through
the Bot API, with strict limits and explicit handling of unsupported/heavy
content. The long-term direction is still to move toward a lightweight Telegram
client experience for Amiga-like systems where the platform constraints and
available protocols make that feasible.

## Goal

Build, one step at a time, a cross-platform Telegram-related lightweight client/tool for:

- AmigaOS 3.x
- MorphOS
- AmigaOS 4.x
- AROS

The project favors portable C code, platform-specific backends and incremental
testing on real hardware or real target systems. The scope is intentionally
limited first to Bot API based diagnostics and lightweight text messaging.

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
- `tg_net`: portable TCP API with MorphOS, AmigaOS 3.x, AmigaOS 4.x and AROS
  backends
- `tg_telegram`: Telegram API response envelope parsing
- `tg_tls`/`tg_https`: minimal TLS/HTTPS with OpenSSL backends for MorphOS and
  AROS, plus AmiSSL backends for AmigaOS 3.x and AmigaOS 4.x
- Bot API `getMe`, `getUpdates` and `sendMessage` helpers; `getUpdates` can
  extract update ids, chat ids and text from the returned array.
- A one-shot echo command can read one update, print the next offset and send an
  `Echo: ...` reply when the update contains text. JSON string escapes are
  decoded before displaying or echoing text.
- A stateful read batch can print up to five pending updates from one
  `getUpdates` response and save the offset without sending replies.
- A bounded read loop can repeat the stateful read flow with caller-chosen
  polling seconds and a maximum iteration count.
- An inbox read command prints pending updates in a more human-readable format
  while reusing the same persistent offset handling and never sending replies.
  It includes Telegram date/time when present, sender, message kind, compact
  one-line output and optional append-only local logging.
- A stateful echo batch can process up to five pending updates from one
  `getUpdates` response, saving the offset after each handled update.
- A bounded echo loop can repeat the stateful batch flow with caller-chosen
  polling seconds and a maximum iteration count.
- Default-token command variants can load `telegram-token.txt` from the active
  data directory, or from a path supplied with `--token-file`.
- `--telegram-preflight` checks the default token path and verifies HTTPS
  reachability to Telegram without sending the token.
- `--telegram-client-console` starts a small manual text console using the
  default token, offset, inbox log and chat-state files. It never sends
  automatically; replies require an explicit `r`, `send` or `reply` command
  with a saved chat index and text.

TLS note: current TLS builds use SNI. Certificate validation is now available
as an opt-in path with `--tls-verify`, `--tls-ca-file` and `--tls-ca-path`.
AROS, MorphOS, AmigaOS 3.x and AmigaOS 4.x have passed supervised validation
tests with an explicit CA bundle. Builds without validation are enough for
supervised connectivity tests, not yet for secure use. Run
`--telegram-tls-status` to print this status from a tester binary, and see
`docs/TLS_CERTIFICATES.md` for certificate validation details.

Initial targets:

- MorphOS: TLS, `getMe`, read-only polling, controlled `sendMessage` and TLS
  certificate validation verified on real hardware
- AmigaOS 3.x: TCP/HTTP, AmiSSL HTTPS, `getMe`, read-only polling, controlled
  `sendMessage` and TLS certificate validation verified on Vampire/AmiKit with
  AmiSSL v5
- AmigaOS 4.x: native QEMU build, AmiSSL HTTPS, `preflight`, `getMe`,
  read-only polling, controlled `sendMessage` and TLS certificate validation
  verified; certificate validation requires a correct system date
- AROS: native builds reported working by the community on AROS One 32-bit and
  64-bit; AROS One i386 alt-abiv0 is cross-built from macOS and has passed
  offline self-tests, TCP/HTTP, HTTPS, `preflight`, `getMe`, read-only polling,
  controlled `sendMessage` and TLS certificate validation in a VM. AROS
  x86_64 now has an experimental build file, but no validated package yet.

Build on MorphOS:

```text
System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos run
```

The default MorphOS build has TLS disabled and is suitable for offline
self-tests. The public MorphOS tester package is built with `ENABLE_TLS=1`,
using OpenSSL, and has passed live `preflight`, `getMe` and read-only polling
on real MorphOS. See `docs/MORPHOS_TESTER.md` for target-side test
instructions.

Remote build from the Mac:

```sh
ssh user@<morphos-host> 'System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos run'
```

Build on native AROS:

```text
make -f Makefile.aros all
```

The AROS Makefile uses native `gcc` by default. Cross-builds can override it:

```sh
make -f Makefile.aros CC=i386-aros-gcc all
```

Current AROS One i386 alt-abiv0 builds use `bsdsocket.library` for networking
and OpenSSL from the AROS SDK for TLS. The VM test target has passed offline
self-tests, TCP/HTTP diagnostics, HTTPS, `preflight`, `getMe`, read-only
polling, controlled `sendMessage` and TLS certificate validation with an
explicit CA bundle. See `docs/AROS_TESTER.md` for tester notes and reporting
details.

Experimental AROS x86_64 builds can start from:

```text
make -f Makefile.aros-x86_64 all ENABLE_TLS=1
```

The offline cross-build path is available when `AROS_SDK_ROOT` and
`AROS_TOOLCHAIN` point at a matching AROS x86_64 SDK/toolchain. Runtime
validation uses a hosted AROS TAP endpoint that is available only while the
runtime is running on the Linux server; it is not a permanent public service.
TLS is still pending because OpenSSL headers/libraries must be present in the
x86_64 SDK before `ENABLE_TLS=1` can work. See
`docs/AROS_X86_64_TESTER.md`.

Recommended AROS offline smoke test:

```text
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-client-self-test
telegram-test --telegram-tls-status
telegram-test --net-test example.com 80
telegram-test --http-test example.com 80 /
```

Community feedback so far: AROS One 32-bit has AmiSSL available, while AROS
One 64-bit currently does not. The current maintained tester path uses OpenSSL
on AROS One i386 alt-abiv0; AmiSSL availability may still matter for future
backend variants.

If AROS `make` reports `Clock skew detected`, check the system date/time or
refresh the unpacked source file timestamps. This can happen when archives are
created on another machine and extracted with timestamps that are in the future
for the AROS system clock.

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

The default AmiSSL build asks for the conservative `AMISSL_V340` API, matching
AmiSSL 5.18 or newer. This is intentional: tester binaries should use a normal
system AmiSSL installation instead of requiring the newest SDK runtime to be
copied next to the program. Override `AMISSL_API_VERSION` only when you need to
target a newer runtime explicitly.

The AmiSSL build needs AmiSSL v5 installed on the target system and visible
through `LIBS:`. If AmiSSL is installed in a separate drawer, assign `AmiSSL:`
to that drawer and put `AmiSSL:Libs` first in `LIBS:` before adding `SYS:Libs`.
On AmigaOS 3.x/Vampire, choose the AmiSSL `68020/030/040/080` library variant.
If an older `amisslmaster.library` is already resident after an upgrade, reboot
or run `Avail FLUSH` before testing so `OpenLibrary()` sees the newer master.

AmigaOS 3.x tester package:

```sh
scripts/package-amigaos3-tester.sh
```

The script builds the system-AmiSSL 68k tester and creates a local package under
`build/packages/`. See `docs/AMIGAOS3_TESTER.md` for target-side requirements,
commands and reporting notes. The package does not include Telegram tokens or
AmiSSL runtime files. The package includes `RunAmigaOS3Preflight`, an AmigaDOS
helper that auto-detects common AmiSSL drawers, sets stack, runs `Avail FLUSH`
and starts `--telegram-preflight`.

AROS tester package:

```sh
scripts/package-aros-tester.sh
```

The script cross-builds an AROS One i386 alt-abiv0 tester and creates a local
package under `build/packages/`. The default package keeps TLS disabled. Use
`ENABLE_TLS=1` to build the OpenSSL-enabled AROS tester.

Experimental AROS x86_64 package:

```sh
scripts/package-aros-x86_64-tester.sh
```

This requires a validated AROS x86_64 build environment or an existing binary
passed with `TARGET=...`; it is not a public tested target yet.

Build or package AmigaOS 4.x:

```sh
make -f Makefile.amigaos4 clean all CC=ppc-amigaos-gcc TARGET=build/amigaos4/telegram-test
make -f Makefile.amigaos4 clean all CC=ppc-amigaos-gcc ENABLE_AMISSL=1 TARGET=build/amigaos4/telegram-test
scripts/package-amigaos4-tester.sh
```

The AmigaOS 4.x TCP backend is enabled in native builds. HTTPS is enabled when
building with `ENABLE_AMISSL=1` and requires the OS4 SDK headers plus the
AmiSSL SDK package. On an OS4 target with the SDK installed, the helper scripts
can also be used directly:

```text
Execute scripts/BuildAmigaOS4Offline
Execute scripts/BuildAmigaOS4AmiSSL
```

The QEMU test target has passed native GCC builds, offline self-tests, AmiSSL
HTTPS, `--telegram-preflight`, `--telegram-getme`, read-only polling,
controlled `sendMessage` and TLS certificate validation with an explicit CA
bundle. Certificate validation depends on the system date being correct. See
`docs/AMIGAOS4_TESTER.md`.

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

MorphOS tester package:

```sh
scripts/package-morphos-tester.sh
```

The script packages a MorphOS binary that was built on MorphOS and copied back
to `build/morphos/telegram-test`. The package is written under
`build/packages/`, which is ignored by git, and does not include Telegram tokens
or OpenSSL runtime files.

## Quick Start

For TLS-enabled tester builds on MorphOS, AmigaOS 3.x, AmigaOS 4.x or AROS:

```text
telegram-test --telegram-tls-status
telegram-test --telegram-preflight
telegram-test --telegram-getme-default
telegram-test --telegram-client-default
telegram-test --telegram-chats-default
telegram-test --telegram-reply-default 1 "Hello from Telegram Amiga"
telegram-test --telegram-client-console
```

TLS-enabled builds can request certificate validation by adding `--tls-verify`
and, when platform defaults are not configured, a CA bundle:

```text
telegram-test --tls-verify --tls-ca-file ca-bundle.crt --telegram-preflight
```

Certificate validation requires a correct system date. If validation fails with
an expired or not-yet-valid certificate error, check the target clock first.

For the common tester checklist, see `docs/HOW_TO_TEST.md`.

Before the reply command can work, send a message to the bot from Telegram and
run `telegram-client-default` or `telegram-client-console` so
`telegram-chats.txt` contains at least one saved chat. Chat index `1` is the
most recently updated chat.

Current options:

```text
-h, --help            Show help
-v, --verbose         Enable debug logging
-q, --quiet           Show warnings and errors only
    --data-dir <path> Set application data directory
    --token-file <path>
                      Override default Telegram token file
    --tls-verify      Verify TLS certificate chain and hostname
    --tls-ca-file <path>
                      CA bundle file for --tls-verify
    --tls-ca-path <path>
                      CA directory for --tls-verify
    --inbox-log-file <path>
                      Append read-only inbox items to a local text log
    --chat-state-file <path>
                      Update one-line-per-chat state during inbox reads
    --net-test <host> <port>
                      Test DNS resolution and TCP connection
    --http-test <host> <port> <path>
                      Test TCP connect/send/recv with HTTP/1.0
    --http-post-self-test
                      Run built-in HTTP POST request builder sample
    --https-test <host> <port> <path>
                      Test TLS connect/send/recv with HTTP/1.0
    --telegram-tls-status
                      Print current TLS security status
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
    --telegram-read-once-state-self-test
                      Run built-in read-only stateful update sample
    --telegram-read-once-state <file> <offset-file>
                      Read pending updates and save a persistent offset
    --telegram-read-once-state-default <offset-file>
                      Stateful read pending updates with default token file
    --telegram-read-loop <file> <offset-file> <poll-seconds> <max-iterations>
                      Run bounded stateful read polling
    --telegram-read-loop-default <offset-file> <poll-seconds> <max-iterations>
                      Run bounded stateful read polling with default token file
    --telegram-inbox-self-test
                      Run built-in inbox-format update sample
    --telegram-inbox <file> <offset-file>
                      Print pending updates in inbox format and save offset
    --telegram-inbox-default <offset-file>
                      Inbox read using the default token file
    --telegram-inbox-loop <file> <offset-file> <poll-seconds> <max-iterations>
                      Run bounded inbox polling
    --telegram-inbox-loop-default <offset-file> <poll-seconds> <max-iterations>
                      Run bounded inbox polling with default token file
    --telegram-session <file> <offset-file> <inbox-log> <chats-file>
                      Run one manual-client receive session
    --telegram-session-default <offset-file> <inbox-log> <chats-file>
                      Manual-client receive session with default token file
    --telegram-session-loop <file> <offset-file> <inbox-log> <chats-file> <poll-seconds> <max-iterations>
                      Run bounded manual-client receive polling
    --telegram-session-loop-default <offset-file> <inbox-log> <chats-file> <poll-seconds> <max-iterations>
                      Bounded manual-client polling with default token file
    --telegram-manual-client <file> <offset-file> <inbox-log> <chats-file> <poll-seconds> <max-iterations>
                      Poll read-only updates, then list saved chats
    --telegram-manual-client-default <offset-file> <inbox-log> <chats-file> <poll-seconds> <max-iterations>
                      Manual-client preview with default token file
    --telegram-client-self-test
                      Run built-in simplified client state sample
    --telegram-client <file> [poll-seconds] [max-iterations]
                      Manual-client preview using default local state files
    --telegram-client-default [poll-seconds] [max-iterations]
                      Short manual-client preview with default token and state files
    --telegram-client-console [poll-seconds] [max-iterations]
                      Interactive manual console using default files
    --telegram-chats <chats-file>
                      List chats saved by manual-client sessions
    --telegram-chats-default
                      List chats from the default chat state file
    --telegram-reply-default <index> <text>
                      Send to a saved chat index using default files
    --telegram-echo-once-self-test
                      Run built-in one-shot echo flow sample
    --telegram-echo-once <file> [offset]
                      Read one update and echo its text back
    --telegram-echo-once-default [offset]
                      Echo one update using the default token file
    --telegram-echo-once-state <file> <offset-file>
                      Echo pending updates using a persistent offset file
    --telegram-echo-once-state-default <offset-file>
                      Stateful echo pending updates with default token file
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
    --telegram-send <file> <chat-id> <text>
                      Alias for --telegram-send-message
    --telegram-send-default <chat-id> <text>
                      Alias for --telegram-send-message-default
    --telegram-send-chat <file> <chats-file> <index> <text>
                      Send to a saved chat by 1-based list index
    --telegram-send-chat-default <chats-file> <index> <text>
                      Send to a saved chat index with default token file
    --telegram-send-last-default <text>
                      Send to chat index 1 from the default chat state file
```

`getUpdates` prints the raw Telegram result and, when present, minimal summaries
for up to the first five updates:

```text
telegram update index: 0
telegram update id: 64052626
telegram update chat id: 148319454
telegram update text: /start
```

Useful commands for a first offline run on any target before network or token
tests:

```text
telegram-test --help
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-client-self-test
```

`telegram-echo-once` is intentionally not a permanent loop. Run it again with
the printed `telegram next offset` value to avoid processing the same update
twice. Do not use arbitrary large offsets as a substitute for state; keep and
reuse the actual next offset printed by the program.

For repeated polling runs, prefer `telegram-echo-once-state`. It reads the
offset from a caller-provided text file, processes up to five pending updates
from one `getUpdates` response, and saves the next offset after each successful
send or after deliberately skipping a non-text update.

Use `telegram-read-once-state` when you only want to receive and mark pending
updates as processed. It prints decoded message text, saves the offset after
each update and does not send replies.

Use `telegram-read-loop` when you want bounded receive-only polling. It reuses
the same persistent offset file as `telegram-read-once-state`, sleeps between
iterations when `poll-seconds` is greater than zero, and never sends replies.

Use `telegram-inbox` or `telegram-inbox-loop` when you want the same safe
receive-only behavior with output shaped for reading messages. It prints update
id, date/time when present, chat id, sender name when available, message kind,
decoded text, a compact one-line summary and the saved next offset. Non-text
messages currently print placeholders such as `<photo>`, `<sticker>` or
`<document>`.

Add `--inbox-log-file <path>` to append compact inbox lines to a local text
file while polling. Add `--chat-state-file <path>` to keep a small
one-line-per-chat state file:

```text
telegram-test --inbox-log-file telegram-inbox.log --telegram-inbox-loop-default telegram-offset.txt 5 10
telegram-test --chat-state-file telegram-chats.txt --telegram-inbox-default telegram-offset.txt
```

`telegram-chats.txt` uses this simple format:

```text
<chat-id>|<last-sender>|<last-date>|<last-text-or-placeholder>
```

Use `telegram-session-default` for the current manual-client preview. It reads
pending updates once, saves the offset, appends inbox lines and updates the chat
state file. It does not send replies:

```text
telegram-test --telegram-session-default telegram-offset.txt telegram-inbox.log telegram-chats.txt
```

Use `telegram-session-loop-default` for a bounded manual-client receive loop:

```text
telegram-test --telegram-session-loop-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
```

Use `telegram-manual-client-default` for the current single-command text preview.
It performs the same bounded receive-only polling and then prints the saved chat
list. It still never sends messages automatically:

```text
telegram-test --telegram-manual-client-default telegram-offset.txt telegram-inbox.log telegram-chats.txt 5 10
```

For the simplified default workflow, keep `telegram-token.txt` beside the
binary and run:

```text
telegram-test --telegram-client-default
```

That command uses `telegram-offset.txt`, `telegram-inbox.log` and
`telegram-chats.txt` in the active data directory, with a default bounded poll
of 5 seconds and 10 iterations. You can override just the timing:

```text
telegram-test --telegram-client-default 2 5
```

For a small manual text console, run:

```text
telegram-test --telegram-client-console
```

Console commands are `p`/`poll`/`read` to poll, `l`/`list` to list saved chats,
`i`/`last`/`inbox` to show the last inbox log line, `s`/`status` to show local
status, `chat <index>` to enter a simple line-oriented chat,
`r`/`send`/`reply <index> <text>` to send a controlled reply and `q`/`quit` to
quit. The console uses the same `telegram-offset.txt`, `telegram-inbox.log` and
`telegram-chats.txt` files as `telegram-client-default`. After `read` or
`poll`, it prints the saved chat list automatically and suggests the
`reply <index> <text>` form. Inside chat mode, type normal text to send to the selected chat. The console auto-reads every 5 seconds by default while waiting for input; use `/watch <seconds>` to change the interval, `/watch off` to disable it, or `/read`, `/list`, `/last`, `/status`, `/back` and `/quit`.

List the saved chats:

```text
telegram-test --telegram-chats telegram-chats.txt
telegram-test --telegram-chats-default
```

For manual replies, use the saved chat list when available. Chat index `1` is
the most recently updated chat:

```text
telegram-test --telegram-send-chat-default telegram-chats.txt 1 "Hello from Telegram Amiga"
telegram-test --telegram-reply-default 1 "Hello from Telegram Amiga"
telegram-test --telegram-send-last-default "Hello from Telegram Amiga"
```

The explicit chat-id send command is still available:

```text
telegram-test --telegram-send-default <chat-id> "Hello from Telegram Amiga"
```

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
`max-iterations` or on the first error. Each iteration may process up to five
pending updates. The accepted limits are
`poll-seconds <= 3600` and `1 <= max-iterations <= 10000`.

The same limits apply to `telegram-read-loop`.

Note: through BebboSSH, the remote shell does not always preserve the AmigaDOS
PATH, so the Makefile uses absolute paths to the MorphOS SDK.

Current Makefiles and helpers:

- `Makefile.morphos`
- `Makefile.amigaos3`
- `Makefile.amigaos3-gcc`
- `Makefile.amigaos4`
- `Makefile.aros`
- `Makefile.aros-i386-abiv0`
- `scripts/package-morphos-tester.sh`
- `scripts/package-amigaos3-tester.sh`
- `scripts/package-amigaos4-tester.sh`
- `scripts/package-aros-tester.sh`
- `scripts/BuildAmigaOS4Offline`
- `scripts/BuildAmigaOS4AmiSSL`
- `scripts/RunAmigaOS3Preflight`

## License

This project is distributed under the MIT License. See `LICENSE` for the full
text.
