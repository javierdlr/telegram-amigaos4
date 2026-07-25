# Changelog

Telegram Amiga, a from-scratch native MTProto Telegram client for
AmigaOS 3.x, AmigaOS 4.x, MorphOS and AROS (i386/x86_64).
Dates use YYYY-MM-DD. Each release ships on all five platform lanes
unless noted.

## [Unreleased] - 0.0.8 development

### Added
- Multi-DC downloads: a document stored on another Telegram datacenter now
  downloads from there (per-DC auth key with a one-time handshake, cached in
  `data/telegram-auth-dc<N>.bin`; `FILE_MIGRATE` mid-transfer hops too).
- Local-first search: typing in the sidebar box filters YOUR chats instantly
  from the local cache; the final "Search Telegram..." row (or ENTER with no
  local match) runs the online search.
- Arrow-key navigation: up/down walk the chat list (ENTER opens; SHIFT+arrows
  keep the keyboard transcript scroll) and the search result list.
- Experimental plain-68000 build option (`M68K_CPU=68000`), not part of the
  released packages yet.

### Changed
- File transfers no longer freeze the window: one chunk moves per event-loop
  turn, so you can keep chatting, switch chats and receive messages while a
  file uploads or downloads. Close gadget or ESC cancels the transfer (a
  second close quits).
- File transfers run on their own dedicated connection (second MTProto
  session), no longer interleaved with the live chat session.
- Menus follow the system colours: new-look menubar and the context popup now
  drawn with the screen's own pens (dark stays dark on OS4.1, classic grey
  stays grey elsewhere).
- While a transfer is running the heavy live poll is throttled (the light
  push drain keeps messages flowing), which also speeds the transfer up.

### Fixed
- Aminet only: the 0.0.7 AmigaOS 3.x archive shipped the wrong (AmigaOS 4)
  binary due to a case-insensitive filename collision in the packaging and
  was republished as 0.0.7a (same program, correct 68k binary). The GitHub
  zips were never affected. The packaging now checks the architecture of the
  binary inside every archive.

## [0.0.7] - 2026-07-24

### Added
- Live transfer percentage in the status bar; downloads and uploads are
  cancellable from the close gadget or ESC.
- Reply on double-click of a message bubble.
- Clipboard support: Copy/Cut/Paste in a proper Edit menu (Amiga+C/X/V),
  with mouse or Shift+arrow text selection in the composer.
- Live updates for messages edited on another device, even while typing.
- TUI: file send/download, including Workbench drag-and-drop.
- `TUI_MODE`/`GUI_MODE` icon tooltype to force the client flavour.

### Changed
- Big-file transfers hardened: lost chunks and parts are retried
  automatically at the same offset, stalled links hit a send timeout instead
  of hanging, wedged sockets reconnect (152 MB tested on PPC lanes).
- Upload limit raised (chunked saveBigFilePart): 250 MiB on PPC/AROS,
  125 MiB on m68k.

### Fixed
- Search with accented names.
- AROS x86_64: crash on relaunch after closing the GUI (shared socket
  library was closed per-connection).
- MorphOS: closing the GUI while the link was busy could freeze the machine
  (connection settle before bsdsocket teardown).

## [0.0.6] - 2026-07-14

### Added
- File sharing: download any received file (right-click, Download) and send
  files to the open chat (right-click, Send file...), up to 10 MB.
- Saved Messages pinned self-chat: Telegram cloud as a transfer drawer
  between the Amiga and your phone/PC.
- Iconify (menu item or OS4 titlebar gadget) parks the client on a
  Workbench AppIcon.
- Click places the text caret in the composer and search box; Del
  forward-deletes.

### Changed
- Script-free launch: two icons start the program directly (TelegramAmiga =
  GUI, TelegramAmiga-TUI = console); the IconX launcher scripts are gone.
- The binary is now called TelegramAmiga (was telegram-test).
- Truer avatar colours, rich on RTG screens.

## [0.0.5] - 2026-07-07

### Added
- Real profile-picture avatars in the chat list (instant blurred previews,
  crisp after opening a chat, cached on disk).
- @username autocomplete in groups (type @ in the composer).
- The window remembers its position and size across restarts.
- Own-screen mode (opt-in via `data/telegram-gui-win.txt`).
- `$VER` version tag in every binary.

### Changed
- Tidy program drawer: auxiliary files in `data/`, avatar photos in
  `avatars/`; old installs migrate automatically.
- Stronger first-login randomness (keyboard and mouse feed the RNG).
- Message line breaks and bullet lists render properly.

### Fixed
- A right-click while the client was busy could freeze the whole system
  (IDCMP_MENUVERIFY removed in favour of a dynamic RMBTRAP).
- More robust chat removal.

## [0.0.4] - 2026-07-02

### Added
- Edit and delete your own messages from the right-click context menu
  (with hover highlighting).
- Multi-device sync: messages sent from another device appear live in the
  open chat.

### Changed
- Live read receipts: the two blue ticks flip in real time.
- Message times follow the Amiga system clock, DST included.
- Clearer 2FA login: no cloud password, just press Enter.

## [0.0.3] - 2026-06-27

### Added
- Reply to a message (tap a bubble or right-click, Reply); the quoted line
  shows above your message.
- Real drawn delivery checkmarks: one tick sent, two blue ticks read.
- Floating scroll-to-newest button.

### Changed
- Flicker-free drawing (off-screen double buffering).

## [0.0.2] - 2026-06-24

### Added
- Scroll-to-top history paging (load older messages on demand).
- Online chat search (find and add chats not in the list).
- Persistent unread badges.
- Drag-and-drop chat reorder and removal (persistent).
- Group "is typing" indicator.
- Full chat list fetched on first login.

### Changed
- Long and multi-line messages supported end to end.
- Flashless Workbench launch (no console window flash).

## [0.0.1] - 2026-06-19

First public alphas: AmigaOS 3.x (m68k), AmigaOS 4.x, MorphOS, AROS i386
and AROS x86_64. Native Intuition GUI (chat list, conversation, live
send/receive, typing indicator, read receipts, in-window login) plus the
text-mode TUI, on a shared from-scratch MTProto core with all cryptography
built in (RSA, Diffie-Hellman, SRP/2FA, AES, SHA). In-place updates during
the 0.0.1 window added full-length messages, accented-character send,
online search, emoji-to-emoticon text, unread clearing and media
placeholders, and cured the OS3 window flicker.
