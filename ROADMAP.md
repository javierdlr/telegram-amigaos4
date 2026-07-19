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
- The 0.0.7 candidate adds large-file uploads, GUI text selection and
  Copy/Cut/Paste, TUI file transfer/drop support, live remote edits and
  receive-only updates while composing.
- A Bot-API text path stays available as a fallback for tokens/bots.
- TLS certificate validation has passed a live CA-bundle smoke test on all four
  platforms (see `docs/TLS_CERTIFICATES.md`).
- Ongoing: 0.0.7 hardware validation, broader community testing and packaging
  polish.

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
