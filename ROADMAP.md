<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# Roadmap

Telegram Amiga is a non-commercial community project. The roadmap is deliberately
pragmatic: each phase should produce something that can be compiled and verified
on at least one real Amiga-like platform.

## Phase 1: Portable Base

- Cross-platform project structure
- Separate builds for MorphOS, AmigaOS 3.x, AmigaOS 4.x and AROS
- Common logging and configuration
- Portable TCP API
- Command-line network tests

## Phase 2: HTTP and TLS

- Minimal HTTP over TCP
- Optional TLS/HTTPS backend
- Certificate validation
- OpenSSL/AmiSSL stabilization on MorphOS
- Selection of the most suitable TLS library for AmigaOS 3.x

## Phase 3: Telegram API

- HTTPS calls to the Bot API or another Telegram API suitable for the target
- Minimal JSON parsing
- Account/token configuration handling
- First message receive tests
- Inbox-format receive-only polling with persistent offsets

## Current Status

The project has moved past the Bot-API diagnostic tester to a real MTProto
client:

- MTProto human releases ship for AmigaOS 3.x, AmigaOS 4.x, MorphOS,
  AROS i386 and AROS x86_64: login wizard, 2FA/SRP, saved chat list,
  text chat and file transfer.
- Release 0.0.7 adds: robust big-file transfers (live %, chunk retry,
  cancellable, send timeouts on stalled links), GUI text selection and
  Copy/Cut/Paste in an Edit menu, reply on double-click, TUI file
  transfer/drop support, live remote edits and receive-only updates while
  composing, accented-name search, and a TUI_MODE icon tooltype.
- Release 0.0.8 (transfers 2.0): non-blocking transfers (use the client
  while a file moves) with pipelined downloads and drag-and-drop upload,
  multi-DC downloads and avatars, local-first search with a two-stage
  online search and a browse of the full dialog list, clickable URLs,
  system-coloured menus, chat list reload with a memory of the chats you
  removed, a configurable download drawer, and AfA_OS (AmiKit)
  compatibility. First release cycle with an adversarial review pass.
- In development (0.0.9): hidden chats now surface directly in the local
  search filter; forwarding to Saved Messages and to a picker-selected chat
  is implemented; bounded photo thumbnails render inline through the in-tree
  JPEG decoder and a geometry-independent cache; and JPEGs can be uploaded as
  native Telegram photos from the GUI, drop requester or TUI. All changes are
  on `main` and still require the complete five-lane real-system validation
  before release.
  System datatypes remain an optional OS4-side optimization, while the
  zero-install in-binary decoder stays the portable base path.
- Then 0.1.0, the first BETA: same program, a different promise. It ships
  once forwarding and inline photos are in, no known freeze remains on any
  of the five platforms, an adversarial review pass has run, and a full
  cycle has gone by without field regressions.
- Later: per-chat file browser, multi-message selection, archive management.
- A Bot-API text path stays available as a fallback for tokens/bots.
- TLS certificate validation has passed a live CA-bundle smoke test on all four
  platforms (see `docs/TLS_CERTIFICATES.md`).
- Ongoing: broader community testing, reliability work on slow links and
  packaging polish for future releases.

## Phase 4: User Interface

- Initial text interface for debugging
- Common UI abstraction
- Platform-specific UI backends
- Native experience for MorphOS and AmigaOS where possible

## Phase 5: Usable Client

- Supported chat or conversation list
- Reading and sending messages
- Minimal local persistence
- Packaging for the supported platforms

## Planned: two-step verification on slow 68k

Signing in with Two-Step Verification derives the key with PBKDF2 (100000
iterations of SHA-512). On a stock 14 MHz 68020 that is roughly forty
minutes, and Telegram expires the SRP challenge long before it ends, so the
password can never be checked (a field report from a 68020 saw exactly that:
no error, just an expired session at the end of the wait).

The wait itself cannot shrink much -- the HMAC midstates are already
precomputed, so each iteration is down to two block transforms. What can
change is the ORDER. The slow derivation depends only on the password and
the account salts, both stable; only srp_id and srp_B expire. So:

1. fetch `account.getPassword`, keep the salts;
2. run the PBKDF2 derivation;
3. fetch `account.getPassword` again for a fresh srp_id and srp_B;
4. compute the SRP proof (short exponents, comparatively quick) and send
   `auth.checkPassword` straight away.

That turns a guaranteed failure into a long but completable login. It needs
`tg_mtproto_srp_make_proof` split into a derivation step and a proof step.

## Planned: photo speed under AfA_OS

Same PiStorm board, two operating systems, two very different speeds: photo
loading under AmiKit (which runs AfA_OS) is noticeably slower than on a
Vampire, while CaffeineOS on that same PiStorm is quick. The hardware is
therefore innocent; something in the AfA_OS graphics path is costing us.

First step is cheap and decisive: run both with `--gui-live-debug` and
compare which replay path each one reports. If AmiKit logs the pen path
where CaffeineOS logs the CyberGraphX one, the RGB888 self-check is failing
under AfA_OS and the fix belongs there (why it fails, and whether the window
RastPort is usable when the off-screen buffer is not). If both report the
same path, the cost is elsewhere -- most likely in the AfA bitmap-text
compatibility work, which already replaces Text() with BltTemplate glyph
runs and is the one thing this configuration does differently.

## Initial Non-Goals

- End-to-end encryption for secret chats
- Full support for heavy media
- Complete compatibility with every feature of modern Telegram clients
