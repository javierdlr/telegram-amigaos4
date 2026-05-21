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

License: MIT

Telegram Amiga is a non-commercial community project, created as a gift to the
Amiga community and as an exploration of what a practical modern messaging
client can look like on constrained and historical platforms.

## Direction

The main development line is MTProto.

The goal is a usable text-first Telegram client for:

- AmigaOS 3.x
- MorphOS
- AmigaOS 4.x
- AROS i386 and x86_64

The Bot API support remains in the tree because it is useful for diagnostics,
TLS/HTTP validation, simple bot-based chat tests and fallback experiments. It
is no longer the main product direction.

## What Works Today

MTProto account mode currently supports:

- MTProto auth-key creation;
- interactive phone/code login wizard;
- 2FA password check when Telegram requires it;
- saved auth state in `telegram-auth.bin`;
- saved DC mismatch protection;
- read-only login smoke tests;
- dialog/peer listing into `telegram-peers.txt`;
- cached user-peer selection;
- reading peer history text;
- sending text to a cached user peer;
- interactive chat mode with `me:` and peer-name transcript lines;
- auto-read while waiting for keyboard input;
- `/read`, `/watch`, `/peer`, `/peers` and `/quit` chat commands;
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

## What It Is Not Yet

This is not yet a full Telegram Desktop/mobile replacement.

Missing or incomplete areas include:

- full update loop based on Telegram updates/differences;
- robust account session management for long daily use;
- groups/channels beyond early peer handling;
- message edits/deletes/reactions;
- media download/upload;
- contact management;
- polished UI;
- broad real-hardware validation across every platform combination.

The current target is a dependable text client first. Heavy media and rich UI
come later, only if the platform constraints make them realistic.

## Public Releases

Current cross-platform pre-alpha release set:

- [AmigaOS 3.x pre-alpha tester 2c6d3c8](https://github.com/kaffeine1/telegram-amiga/releases/tag/amigaos3-prealpha-20260521-2c6d3c8)
- [MorphOS pre-alpha tester 2c6d3c8](https://github.com/kaffeine1/telegram-amiga/releases/tag/morphos-prealpha-20260521-2c6d3c8)
- [AmigaOS 4.x pre-alpha tester 2c6d3c8](https://github.com/kaffeine1/telegram-amiga/releases/tag/amigaos4-prealpha-20260521-2c6d3c8)
- [AROS i386 ABIv0 TLS pre-alpha tester 2c6d3c8](https://github.com/kaffeine1/telegram-amiga/releases/tag/aros-i386-abiv0-tls-prealpha-20260521-2c6d3c8)
- [AROS x86_64 offline pre-alpha tester 2c6d3c8](https://github.com/kaffeine1/telegram-amiga/releases/tag/aros-x86_64-offline-prealpha-20260521-2c6d3c8)

Packages include platform README files, `USER_RUNBOOK.md`, MTProto login notes
and helper scripts. They do not include local tokens, API credentials, auth
files, phone-code files, passwords or peer caches.

## Quick Start: MTProto Account Chat

Use a test Telegram account when possible. This path sends real Telegram
messages.

Create `telegram-api.txt` next to `telegram-test`:

```text
<api_id>
<api_hash>
```

Get those values from Telegram's API development page. Keep them private.

Run the login wizard:

```text
Execute RunMTProtoLoginWizard
```

The wizard asks for phone number, Telegram login code and optional 2FA
password. Some retro consoles may echo password input, so avoid doing this
while screen-sharing.

After login:

```text
Execute RunMTProtoCheckLocal
Execute RunMTProtoInspectAuth
Execute RunMTProtoLoginSmoke
Execute RunMTProtoListPeers
Execute RunMTProtoChat
```

Inside chat mode:

```text
/read              poll immediately
/watch 5           auto-read every 5 seconds
/watch off         disable auto-read
/peer              choose another peer
/peers             refresh the peer cache
/quit              exit
```

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
telegram-api.txt
telegram-auth.bin
phone-code-hash.txt
telegram-password.txt
telegram-peers.txt
telegram-phone.txt
telegram-code.txt
```

If a Bot API token is exposed, revoke it with BotFather. If account login
material is exposed, treat the Telegram account as compromised and rotate what
can be rotated.

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

AROS x86_64 offline package:

```sh
AROS_TOOLCHAIN=/path/to/aros-x86_64-toolchain \
AROS_SDK_ROOT=/path/to/AROS/Development \
scripts/package-aros-x86_64-tester.sh
```

Offline smoke test on a local/native-style build:

```sh
make -f Makefile.aros clean all ENABLE_GZIP=0 ENABLE_GZIP_PUFF=1
./build/telegram-test --mtproto-self-test-fast
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
