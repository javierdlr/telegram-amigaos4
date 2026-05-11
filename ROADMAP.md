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

## Near-Term Technical Milestones

- Keep MorphOS and AmigaOS 3.x at the same verified level: TLS preflight,
  `getMe`, receive-only polling and offset persistence.
- Continue improving inbox output into the first practical text-reading
  workflow. Current command-line inbox output already includes date/time,
  sender, message kind, compact summaries and optional append-only logs.
- Add controlled send-message workflows without automatic replies by default.
  Current command-line aliases support manual sends after reading a chat id or
  by selecting a saved chat from the local chat list.
- Add small local conversation state files once the receive/send command line
  flows are stable. Current append-only inbox logs and one-line chat state are
  the first step.
- The first manual-client preview now exists as `telegram-session-default`,
  combining one receive pass, offset persistence, inbox logging and
  one-line-per-chat state without sending replies.
- A bounded manual-client loop and simple saved-chat listing now exist through
  `telegram-session-loop-default`, `telegram-chats` and
  `telegram-send-chat-default`.
- A first single-command text preview now exists as
  `telegram-manual-client-default`: it polls read-only updates, updates local
  inbox/chat files and prints the saved chat list without automatic sends.
- A shorter default workflow now exists as `telegram-client-default`,
  `telegram-chats-default`, `telegram-reply-default` and
  `telegram-send-last-default`, using default local state files and keeping the
  most recently active chat at index 1.
- A first interactive manual console now exists as `telegram-client-console`.
  It can poll, list saved chats, send an explicit indexed reply and quit.
- TLS security status is explicit through `telegram-tls-status`; certificate
  validation is still a required future task before normal secure use.
- AROS now has an initial BSD-socket TCP backend that needs native AROS
  validation before HTTPS/TLS work starts.
- Keep AROS and AmigaOS 4.x buildable while their networking/TLS backends are
  developed with community or hardware feedback.
- Bring the new AmigaOS 4.x/QEMU target to the offline self-test level first:
  install or provide a PPC AmigaOS 4 toolchain, build the stub backend, then
  run parser/manual-client self-tests before implementing TCP/TLS.

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
