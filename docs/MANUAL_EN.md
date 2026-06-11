# Telegram Amiga — Quick Manual (English)

*Versione italiana: [MANUALE_IT.md](MANUALE_IT.md)*

Real Telegram, in a console window, on your Amiga. No browser, no external
proxies: the client speaks MTProto directly to the Telegram servers. It
runs on AmigaOS 3.x (68k), AmigaOS 4.x, MorphOS and AROS i386. This is
experimental (pre-alpha) software: use it sensibly.

## 1. Download

Grab the zip for your platform from the latest release:
https://github.com/kaffeine1/telegram-amiga/releases

- `Telegram-amigaos3-<date>.zip` — AmigaOS 3.x, 68020+, no ixemul needed
- `Telegram-amigaos4-<date>.zip` — AmigaOS 4.x PPC
- `Telegram-morphos-<date>.zip` — MorphOS PPC
- `Telegram-aros-i386-<date>.zip` — AROS x86 ABIv0

## 2. Install

Unpack anywhere (RAM: is fine for a quick try, a folder on disk if you
want your login to survive a reboot). Inside you will find:

- `TelegramAmiga` + icon — the program to start (double-click)
- `telegram-test` — the actual binary
- `telegram-api.txt` — the project's public API credentials (ready to go)
- `README.txt` — instructions

## 3. First start and login

Double-click **TelegramAmiga**. On the first run the login wizard starts:
enter your phone number in international format (+44...), then the code
Telegram sends to your official app, and your 2FA password if you use
one. The login is saved in `telegram-auth.bin`: from the second start you
go straight in.

Tip: if in doubt, try with a secondary account first.

## 4. The screen

Full-window layout, AmIRC/XChat style:

- the **status bar** at the top with the open chat's name
- the **conversation** scrolling in the middle (`[HH:MM]` timestamps,
  day separators, emoji rendered as classic emoticons)
- the **input line** at the bottom, always in place

Type and press Enter to send ([ok] = accepted by the server). Enter on an
empty line = read new messages now (they arrive on their own anyway).

## 5. Switching chats quickly

- **F1..F10** → chat 1..10 (with Shift: 11..20)
- **Tab** → back to the previous chat (and forth)
- **a number + Enter** → chat with that index
- `/peers` → chat list with indexes and unread counters
- `/add name` (or @username or a t.me link) → search Telegram and add
- `/search text` → search among the chats already listed

When a message arrives from another chat you get a line like
`[3] John Smith: hi...` and the screen flashes: press F3 (or type 3) and
you are there.

## 6. Useful commands

- `/history` — recent messages of the open chat
- `/watch sec` — how often to poll for new messages (`/watch off`)
- `/bell` — notification flash on/off
- `/color` — colours on/off
- `/resize` — redraw the layout after you resized the window (needed on
  terminals that do not announce resizes, e.g. the MorphOS shell)
- `/remove n` — drop a chat from the list
- `/help` — the full list
- `/quit` — exit (**Ctrl+C** works too, everywhere)

You can resize the window at any time: the layout re-fits itself.

## 7. Platform notes

- **AmigaOS 3.x**: dark theme with full-row background. Tested on 68030+;
  needs a TCP stack (Roadshow, AmiTCP...).
- **AmigaOS 4.x**: everything enabled by default.
- **MorphOS**: colours are off by default and cross-chat notifications
  are temporarily disabled (console/network quirks of the platform);
  auto-read every 12s — tune with `/watch`. After enlarging the window,
  type `/resize`. Run it from disk (e.g. `Work:`), not from RAM:.
- **AROS**: if the icon-launched console cannot do full-screen, the
  client detects it and falls back to the classic line-by-line flow.

## 8. Upgrading from a previous version

Unpack the new version and copy your `telegram-auth.bin` (login) and
`telegram-peers.txt` (chat list) from the old folder into the new one.
Done: no new login needed.

## 9. Security, please

- **Never publish** `telegram-auth.bin`: it is your session.
- No screenshots with phone numbers, login codes or private messages.
- The credentials in `telegram-api.txt` are the project's public ones:
  power users can switch to their own (see README).

## 10. Problems?

Report in the group with a screenshot and your machine/OS model: that is
how nearly every bug so far was found and fixed. Happy chatting from your
Amiga! 🖥️
