<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# Telegram Amiga

Telegram Amiga is a pre-alpha effort to build a serious lightweight Telegram
text client for Amiga-like systems.

The project target is not a toy demo and not just a bot helper: the main
direction is now an MTProto client that can log in with a normal Telegram
account, list peers and exchange text messages with people from an Amiga-style
console.

Current status: **working pre-alpha MTProto text-chat prototype**.

> **This is the `gui-intuition` development branch.** On top of the released
> console/TUI client it adds a second, native Intuition GUI front-end (Phase
> 5b). The GUI is at milestone 0 — a real window that renders the chat layout
> on real hardware — and is not yet wired to live Telegram data. The stable
> client lives on `main`. See
> [docs/GUI_ARCHITECTURE.md](docs/GUI_ARCHITECTURE.md).

License: MIT

Telegram Amiga is a non-commercial community project, created as a gift to the
Amiga community and as an exploration of what a practical modern messaging
client can look like on constrained and historical platforms.

Development diary and lab notes:
https://androidlab.it/telegram-amiga-diario-sviluppo-client-mtproto/

## Direction

The main development line is MTProto.

The goal is a usable text-first Telegram client for:

- AmigaOS 3.x (native build: no ixemul.library and no AmiSSL required — all
  crypto is in-tree; needs a 68020+ CPU)
- MorphOS
- AmigaOS 4.x
- AROS i386
- AROS x86_64

The Bot API support remains in the tree because it is useful for diagnostics,
TLS/HTTP validation, simple bot-based chat tests and fallback experiments. It
is no longer the main product direction.

AROS x86_64 is a released platform: the client logs in and chats live on
hosted AROS x86_64, built against a matching trunk SDK, and the release
binary follows that ABI. AROS One v0.38 pairs a different kickstart with its
SDK and does not run it (it faults before `main`); that distribution is out
of scope for this lane. AROS i386 ABIv0 remains the broadest AROS build.

Two front-ends, one core: a **console/TUI** client (works everywhere — the
low-end and universal-fallback choice) and a **native Intuition GUI** (a
modern desktop experience, offered on every platform including fast 68k, with
the user picking at launch). Both share the same MTProto core; neither needs
ixemul, AmiSSL or MUI. The GUI is in development on this branch — see below.

## What Works Today

MTProto account mode currently supports:

- MTProto auth-key creation;
- interactive phone/code login wizard;
- 2FA password check when Telegram requires it;
- saved auth state in `telegram-auth.bin`;
- saved DC mismatch protection;
- read-only login smoke tests;
- dialog/peer listing into `telegram-peers.txt`;
- cached conversation peer selection;
- reading peer history text for users, basic groups and channels/supergroups;
- sending text to cached users, basic groups and channels/supergroups when the
  account has permission;
- interactive chat mode with peer-name transcript lines, running in a
  full-screen console layout (status bar, scrolling transcript, fixed input
  line; `--ui-tui off` returns to the linear flow);
- coloured high-contrast chat screen (dark theme) with emoji-to-emoticon
  rendering, [HH:MM] timestamps, day separators and real line breaks in
  messages;
- live cross-chat notifications: messages arriving in other chats (users,
  groups, channels) show as highlighted lines with the chat number to jump
  there;
- quick chat switching: function keys F1..F10, Tab back to the previous
  chat, bare chat numbers;
- Up/Down command history and auto-read while waiting for keyboard input;
- `/peers`, `/search`, `/add`, `/remove`, `/watch`, `/color`, `/help` and
  `/quit` chat commands;
- `gzip_packed` MTProto history responses without requiring zlib on targets
  that use the embedded `puff` fallback.

Bot API mode currently supports:

- Telegram HTTPS preflight;
- `getMe`;
- `getUpdates`;
- controlled `sendMessage`;
- read-only inbox/log state;
- saved chat list;
- terse bot-based human chat mode.

TLS support exists through OpenSSL or AmiSSL depending on the target. Certificate
validation is opt-in with `--tls-verify`, `--tls-ca-file` and `--tls-ca-path`.
See [TLS_CERTIFICATES.md](docs/TLS_CERTIFICATES.md).

## Native GUI (Phase 5b, in development)

A second front-end is taking shape on this branch: a real Intuition window
instead of the console. The design is "modern AmIRC" with cues from current
Telegram — a chat list on the left (initials avatars, last-message preview,
unread badges), a conversation pane with per-sender colours and message
bubbles, an input line with a Send button, all in a dark theme — drawn by the
client itself on a RastPort, with **zero external dependencies** (Intuition
raw + GadTools, no MUI).

Milestone 0 is done: `--gui-test` opens the window, renders the full layout
with demo data, measures its own redraw time and runs an IDCMP event loop
(close gadget / Q / live resize). It is validated end to end on AROS x86_64
and on real AmigaOS 3.x hardware (a 68080/Vampire: ~9 ms for a full-window
repaint, ~235 KB footprint). It builds on all five targets; the AmigaOS 4
branch uses the interface model, the others the classic shared-library bases.

What the GUI does **not** do yet: show live Telegram data. That arrives once
the shared chat engine is extracted from the console path (in progress) so
both front-ends sit on the same model. The portable layout maths run headless
with `--gui-self-test`, and the engine extraction is checked by
`--chat-engine-self-test` (both in CI). Full plan and status:
[docs/GUI_ARCHITECTURE.md](docs/GUI_ARCHITECTURE.md).

## What It Is Not Yet

This is not yet a full Telegram Desktop/mobile replacement.

Missing or incomplete areas include:

- full update loop based on Telegram updates/differences;
- robust account session management for long daily use;
- full group/channel management beyond basic text history and send;
- message edits/deletes/reactions;
- media download/upload;
- contact management;
- polished UI;
- broad real-hardware validation across every platform combination.

The current target is a dependable text client first. Heavy media and rich UI
come later, only if the platform constraints make them realistic.

## Public Releases

Current human packages include a one-click launcher, icon, README and a bundled
public `telegram-api.txt` for TelegramAmiga. They do not include local tokens,
auth files, phone-code files, passwords or peer caches.

Latest human releases:

- [AmigaOS 3.x 20260611 (native — no ixemul / no AmiSSL; full-screen chat, solid)](https://github.com/kaffeine1/telegram-amiga/releases/tag/amigaos3-20260611)
- [MorphOS 20260611 (full-screen chat, solid)](https://github.com/kaffeine1/telegram-amiga/releases/tag/morphos-20260611)
- [AmigaOS 4.x 20260611 (full-screen chat, solid)](https://github.com/kaffeine1/telegram-amiga/releases/tag/amigaos4-20260611)
- [AROS i386 ABIv0 20260611 (full-screen chat, solid)](https://github.com/kaffeine1/telegram-amiga/releases/tag/aros-i386-20260611)
- [AROS x86_64 20260612 (first public build)](https://github.com/kaffeine1/telegram-amiga/releases/tag/aros-x86_64-20260612)

The AROS x86_64 package targets trunk-SDK-matched systems (hosted AROS
x86_64 is the validated reference). It does not run on AROS One v0.38.

## Quick Start: MTProto Account Chat

Use a test Telegram account when possible. This path sends real Telegram
messages.

Human release packages already include `telegram-api.txt` with the public app
credentials for TelegramAmiga. Advanced testers may replace it with their own
two-line Telegram API file if needed.

Start the MTProto client:

```text
Execute TelegramAmiga
Execute RunMTProtoStart
```

If no saved login exists, this starts the phone/code login wizard first. After
login it uses the DC stored in `telegram-auth.bin`, refreshes the peer cache
and enters chat mode. The peer list can include users, basic groups and
channels/supergroups; sending to channels depends on the Telegram permissions
of the logged-in account.

Release packages include an IconX project icon named `TelegramAmiga.info`
next to `TelegramAmiga`. Double-clicking that icon starts the same flow;
`Execute TelegramAmiga` is the portable fallback when icon or protection
metadata is lost during unpacking.

Manual login is still available:

```text
Execute RunMTProtoLoginWizard
```

The wizard asks for phone number, Telegram login code and optional 2FA
password. Some retro consoles may echo password input, so avoid doing this
while screen-sharing.

Manual validation/debug commands:

```text
Execute RunMTProtoCheckLocal
Execute RunMTProtoInspectAuth
Execute RunMTProtoLoginSmoke
Execute RunMTProtoListPeers
Execute RunMTProtoChat
```

Inside chat mode:

```text
F1..F10            jump to chat 1..10 (Shift+F1..F10: chats 11..20)
Tab                back to the previous chat (also /swap)
3                  switch to chat number 3
Enter              read new messages now
/peers             show the chat list (current chat marked with *)
/search text       find a cached chat by name
/add name          search Telegram and add a chat
/remove n          remove cached chat n
/history           show recent messages again
/watch 2           auto-read every 2 seconds (/watch off disables)
/color             toggle colours (/color on|off)
/bell              toggle the notification flash/bell
/help              show commands
/quit              exit
```

Up/Down arrows recall typed lines. The chat runs full screen -- status bar
on top, transcript in the middle, fixed input line at the bottom -- on a
black high-contrast screen with bold sender names; multi-line messages keep
their line breaks and emoji render as text emoticons (`:)` `<3` `(y)`) since
Amiga consoles have no emoji glyphs. `--ui-tui off` returns to the classic
linear flow, `--ui-theme plain` keeps the normal window colours,
`--ui-color off` disables colours entirely. Text size follows the system
console font preferences. Platform notes: on MorphOS auto-read runs at a
slower pace, cross-chat notifications arrive at a paced rhythm (within
~12 seconds via a small updates.getDifference drain that protects the
slow TCP stack; `/diff off` disables it) and colours stay off (console
issue under investigation).

If a command reports `auth-dc-mismatch`, inspect the saved auth file and run
with the matching DC endpoint. The latest AmigaOS 3.x validation used:

```text
Execute RunMTProtoChat 149.154.167.91 443 4 telegram-api.txt telegram-auth.bin telegram-peers.txt telegram-test
```

Detailed MTProto docs:

- [MTPROTO_QUICK_TEST.md](docs/MTPROTO_QUICK_TEST.md)
- [MTPROTO_REAL_LOGIN.md](docs/MTPROTO_REAL_LOGIN.md)
- [MTPROTO_BOOTSTRAP.md](docs/MTPROTO_BOOTSTRAP.md)

## Privacy And Local Files

Never publish these files or screenshots/logs that reveal their contents:

```text
telegram-token.txt
telegram-auth.bin
phone-code-hash.txt
telegram-password.txt
telegram-peers.txt
telegram-phone.txt
telegram-code.txt
telegram-seed.bin
```

If a Bot API token is exposed, revoke it with BotFather. If account login
material is exposed, treat the Telegram account as compromised and rotate what
can be rotated.

## Randomness, Honestly

On platforms without a kernel CSPRNG or a TLS library (AmigaOS 3, AROS,
MorphOS without OpenSSL, AmigaOS 4 when AmiSSL is missing) the MTProto
secrets come from an in-tree hash-DRBG seeded from local entropy: CPU
timer jitter (TSC / E-Clock / PPC timebase), wall clock, run-varying
addresses, plus two sources added after a community security review:

- `telegram-seed.bin` (next to the binary) persists DRBG output across
  runs, Linux random-seed style, so entropy accumulates over time instead
  of restarting from a cold boot state. It is mixed into the pool at
  startup and immediately overwritten with fresh output.
- The timing of your keystrokes (the login wizard included) is folded
  into the generator at every use.

Honest limit: inside an emulator or VM the timer jitter is weaker and
boot states are more reproducible than on real hardware. The first run in
a fresh VM (no seed file yet, few keystrokes) is the weakest moment, and
that is exactly when the auth-key DH secret is generated. If your threat
model is more than hobbyist, do the first login on real hardware, or use
a platform with AmiSSL/OpenSSL, or accept the risk consciously. After the
first run the seed file makes every later state depend on accumulated
history.

## Keyboard Layouts And Accented Characters

TelegramAmiga reads text from the system console. Keyboard layout handling is
done by the Amiga-like operating system, not by the client.

If typed keys do not match the characters shown on screen, set the correct
system keymap first, for example through the platform's input preferences or
startup configuration. This is especially important when testing accented
characters such as `à`, `è`, `ì`, `ò` and `ù`, and when using VNC, QEMU or
remote-desktop tools that may apply their own keyboard mapping.

## Bot API Fallback Mode

Bot API mode is still useful for target validation and bot-based experiments.

Create `telegram-token.txt` next to `telegram-test`, containing only the bot
token, then run:

```text
Execute RunAmigaOS3Preflight
Execute RunAmigaOS3GetMe
Execute RunAmigaOS3HumanChat
```

Equivalent helpers exist for other targets:

```text
Execute RunMorphOSPreflight
Execute RunMorphOSGetMe
Execute RunMorphOSHumanChat

Execute RunAmigaOS4Preflight
Execute RunAmigaOS4GetMe
Execute RunAmigaOS4HumanChat

Execute RunAROSPreflight
Execute RunAROSGetMe
Execute RunAROSHumanChat
```

For the shared user guide, see [USER_RUNBOOK.md](docs/USER_RUNBOOK.md).

## Platform Notes

- [AmigaOS 3.x tester notes](docs/AMIGAOS3_TESTER.md)
- [MorphOS tester notes](docs/MORPHOS_TESTER.md)
- [MorphOS cross-build notes](docs/MORPHOS_CROSS_BUILD.md)
- [AmigaOS 4.x tester notes](docs/AMIGAOS4_TESTER.md)
- [AROS tester notes](docs/AROS_TESTER.md)
- [AROS x86_64 tester notes](docs/AROS_X86_64_TESTER.md)
- [Native GUI architecture (Phase 5b)](docs/GUI_ARCHITECTURE.md)
- [Common test checklist](docs/HOW_TO_TEST.md)
- [Current test matrix](docs/TEST_MATRIX.md)

## Build And Package

AmigaOS 3.x package:

```sh
scripts/package-amigaos3-tester.sh
```

MorphOS cross-build and package:

```sh
scripts/docker-morphos-cross-build.sh
scripts/package-morphos-tester.sh
```

AmigaOS 4.x package expects an OS4 binary in `build/amigaos4/telegram-test`:

```sh
scripts/package-amigaos4-tester.sh
```

AROS i386 ABIv0 TLS package:

```sh
ENABLE_TLS=1 scripts/package-aros-tester.sh
```

AROS x86_64 diagnostic package (build against the SDK of the runtime you
target — kickstart and SDK must come from the same line, see
`docs/AROS_X86_64_TESTER.md`):

```sh
AROS_TOOLCHAIN=/path/to/aros-x86_64-toolchain \
AROS_SDK_ROOT=/path/to/AROS/Development \
scripts/package-aros-x86_64-tester.sh
```

The human-facing x86_64 package comes from `scripts/package-human-release.sh`
with `AROS_X86_64_BINARY` pointing at the SDK-matched build.

Offline smoke test on a local/native-style build:

```sh
make -f Makefile.aros clean all ENABLE_GZIP=0 ENABLE_GZIP_PUFF=1
./build/telegram-test --mtproto-self-test-fast
```

Native GUI and portable self-tests:

```sh
telegram-test --gui-test            # native demo window (Amiga; host prints a notice)
telegram-test --gui-self-test       # portable GUI layout check (host/CI)
telegram-test --chat-engine-self-test  # chat-engine invariants (host/CI)
```

## Repository Structure

- `core/`: portable client, Bot API and MTProto logic
- `include/`: internal public interfaces
- `platforms/`: platform-specific TCP/TLS/input backends
- `scripts/`: build, package and target helper scripts
- `docs/`: platform notes, runbooks and test matrix
- `third_party/puff/`: small inflate fallback used for gzip-packed MTProto
  responses when zlib is unavailable
- `src/main.c`: thin entry point

Important modules:

- `tg_mtproto_*`: MTProto transport, auth, TL, crypto, session, login and probe
  code
- `tg_chat_engine`: driver-agnostic chat-session model shared by the console
  and GUI front-ends (Phase 5b, under extraction from `tg_mtproto_probe`)
- `tg_console*`: console/TUI front-end (full-screen chat, colours, layout)
- `tg_gui`, `tg_gui_window`: native Intuition GUI front-end (portable renderer
  + per-platform window/RastPort backend)
- `tg_text_client`: Bot API manual text console
- `tg_bot`, `tg_telegram`: Bot API helpers
- `tg_tls`, `tg_https`, `tg_net`: portable networking/TLS layer
- `tg_client_state`, `tg_offset_state`: local state helpers

## AI-Assisted Development

This project is developed with help from AI agents used as engineering tools
for code analysis, implementation, refactoring, packaging, documentation and
test preparation.

The goal is not to replace the experience of the Amiga community. The goal is
to make that experience go further: real machines, emulators, cross-toolchains
and AI-assisted iteration working together on software that would otherwise be
painfully slow to bootstrap.

The public repository should contain only curated code, useful documentation
and logical commits. Local AI diaries, transcripts, secrets, failed attempts and
operational notes stay out of Git.

## Contributing And Testing

Useful reports include:

- platform and version;
- real hardware, hosted, emulated or virtualized;
- CPU/accelerator;
- TCP/IP stack;
- TLS library/version;
- whether `Run...Preflight` works;
- whether MTProto login/list-peers/chat works;
- command output with secrets removed;
- crashes, freezes, requesters or unusual delays.

Do not publish tokens, API credentials, auth files, phone numbers, login codes,
2FA passwords, access hashes or private message text.
