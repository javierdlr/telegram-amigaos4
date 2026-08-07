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

## Planned: link previews (0.0.10 headline candidate)

First field feedback on 0.0.9: link previews pasted into chats show up
for some links and not for others. The ones that appear are not link
previews at all; they are real attached photos with a caption (the usual
news-channel format), which the inline-photo pipeline renders. A bare
pasted link arrives as `messageMediaWebPage`, which the client currently
skips entirely, by design; the bubble shows only the clickable URL.

Plan, in order:

1. Parse `messageMediaWebPage` (TL constructors verified against the
   layer-214 schema first): show the page title or site name plus the
   first description line under the message text. The TUI shows the
   title line only.
2. When the preview carries a photo, feed it to the existing bounded
   photo pipeline (stripped preview, incremental fetch, disk cache), so
   the image costs nothing new and obeys the same inline-photos setting.
3. Optional, only if the cycle has room: handle `updateWebPage`, so a
   preview the server generates late still reaches an open chat.

Even complete, previews will stay per-link: the server builds them from
the target page's metadata, so pages without usable metadata show none
(`webPageEmpty`, same as official clients), pending ones arrive later
(`webPagePending`), and a sender can disable the preview per message.
This moves link previews out of the Tier 4 "degrade to a link line"
non-goal on the strength of the field reports.

## Planned: captions when sending photos

Requested alongside the 0.0.9 feedback: let the user attach a caption to
a photo they send. The wire side is already there; both media writers
(photo and document) call `messages.sendMedia` with an empty caption
string today, so the protocol work is filling one field the client
already sends, plus the composer's existing UTF-8 conversion. Received
captions already render under the inline photo. The real work is the
input surface:

- GUI: whatever sits in the composer when the photo send is confirmed
  becomes the caption, after a one-line confirm requester when the
  composer is not empty (so an unrelated draft is never swallowed).
  Applies to Send photo..., the context-menu entry and the Workbench
  drop alike; the status line says when a caption went along.
- TUI: `/photo <path> [caption words...]`, everything after the path is
  the caption.
- The over-10-MiB fallback that sends a photo as a document carries the
  same caption.

## Planned: two MorphOS popup glitches

Both reported from the field on 0.0.9 and both about the right-click popup
living next to inline photos.

1. Opening the popup beside a photo draws it BEHIND the picture. The window
   paint composes the popup last for exactly this reason, but the photo
   replay is a separate pass that writes straight to the window RastPort
   (the CyberGraphX path) after the blit, so it lands on top. The popup
   needs to be excluded from the replay's dirty rectangles, or repainted
   after them.
2. On an own screen, right-clicking in the conversation to pick an entry
   makes the chat flicker. The popup path repaints more than it needs to
   there; the transcript should keep its pixels while only the popup area
   is composed.

Neither affects the other lanes, where the replay goes through the friend
bitmap before the blit rather than to the window.

## Planned: photo speed under AfA_OS

Rows are handed to cybergraphics in blocks of 8 on the 68k line, which took
the per-slice cost from 220-820 ms down to 0-20 ms on a Vampire. AmiKit still
shows a tail of slow slices (180-620 ms), so the per-call overhead there is
higher than elsewhere. First move, cheap and mechanical: raise
TG_GUI_PHOTO_REPLAY_ROWS from 8 to 16 on m68k (about 12 KB more of staging
buffer) and measure again from the log's "pace replay budget" lines. If the
tail survives that, the cost is not the call count and the investigation
below is the real answer.


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
