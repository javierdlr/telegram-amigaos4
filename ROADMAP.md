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
- In development (0.0.8): non-blocking transfers (use the client while a
  file moves) with pipelined downloads and drag-and-drop upload, multi-DC
  downloads and avatars, local-first search with keyboard navigation,
  clickable URLs, system-coloured menus, chat list reload with a memory of
  the chats you removed.
- Next (0.0.9): forwarding messages, files and media to another chat -- the
  destination picker reuses the local-first search, and a one-click
  "Forward to Saved Messages" archives anything into your own cloud without
  re-uploading it -- plus INLINE PHOTOS in the conversation (today a photo
  is a [Photo] label; the JPEG decoder already in use for avatars grows into
  the bubbles, degrading per platform).
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

## Initial Non-Goals

- End-to-end encryption for secret chats
- Full support for heavy media
- Complete compatibility with every feature of modern Telegram clients
