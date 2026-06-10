<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# Telegram Amiga User Runbook

This is the short path for a human tester who wants to run the current
Telegram Amiga pre-alpha package without learning every diagnostic command.

## 1. Enter The Package Drawer

Copy or unpack the package drawer on the target system, then enter it from a
Shell:

```text
CD Work:TelegramAmiga
```

Use the real drawer name if your unpacker created a longer package directory.

## 2. Add A Bot Token

Create `telegram-token.txt` in the same drawer as `telegram-test`.

The file must contain only the Telegram Bot API token. Do not include quotes,
spaces, shell commands or extra lines. Do not share screenshots or logs that
show the token.

## 3. Run The Friendly Helpers

This is the recommended user-facing path. The longer command-line options are
for diagnostics and bug reports.

Use `Execute`; ZIP extraction can lose script protection bits on Amiga-like
systems.

AmigaOS 3.x:

```text
Execute RunAmigaOS3Preflight
Execute RunAmigaOS3GetMe
Execute RunAmigaOS3HumanChat
```

MorphOS:

```text
Execute RunMorphOSPreflight
Execute RunMorphOSGetMe
Execute RunMorphOSHumanChat
```

AmigaOS 4.x:

```text
Execute RunAmigaOS4Preflight
Execute RunAmigaOS4GetMe
Execute RunAmigaOS4HumanChat
```

AROS:

```text
Execute RunAROSPreflight
Execute RunAROSGetMe
Execute RunAROSHumanChat
```

`Preflight` checks HTTPS reachability without using the bot token. `GetMe`
checks that the token works. `HumanChat` starts the terse back-and-forth chat
and is the main mode for human testing.

## 4. Human Chat Behavior

Human chat is intentionally quiet:

- it does not print a repeated `>` prompt;
- it does not print periodic blank lines while waiting;
- it prints chat text only when a message is received or sent;
- it keeps diagnostic log lines out of the chat transcript;
- it still appends `telegram-inbox.log` for later debugging.

Type normal text to send a message. Press Enter on an empty line to check for
new replies immediately. Type `quit` to exit.

If no chat is selected yet, send a message to the bot from Telegram and press
Enter in the Amiga Shell. If you already know the Bot API chat id, you can type
that id once to select it.

## 5. Files Created Next To The Program

The default user flow keeps state in the package drawer:

```text
telegram-token.txt
telegram-offset.txt
telegram-inbox.log
telegram-chats.txt
telegram-selected-chat.txt
```

Keep `telegram-token.txt` private. The other files are local runtime state and
can be removed when you want a fresh test run.

## 6. MTProto Account Login And Text Chat

The MTProto path logs in with a normal Telegram account instead of a bot. It is
pre-alpha and should be tested with an account you are comfortable using for
experiments.

Create `telegram-api.txt` next to `telegram-test`:

```text
<api_id>
<api_hash>
```

Get these values from Telegram's API development page. Keep them private. Do
not publish `telegram-api.txt`, `telegram-auth.bin`, `phone-code-hash.txt`,
`telegram-password.txt`, `telegram-peers.txt`, screenshots showing phone
numbers, login codes, contacts or message text.

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
next to `TelegramAmiga`. Double-click `TelegramAmiga` to start the same flow.
Some ZIP extractors and hosted filesystems drop Amiga icon/protection metadata,
so `Execute TelegramAmiga` remains the portable fallback.

Manual login is still available:

```text
Execute RunMTProtoLoginWizard
```

The wizard asks for phone number, Telegram login code and, if required, 2FA
password. On some retro consoles the password can be visible while typed, so do
not run this while screen-sharing.

Manual validation/debug commands:

```text
Execute RunMTProtoCheckLocal
Execute RunMTProtoInspectAuth
Execute RunMTProtoLoginSmoke
Execute RunMTProtoListPeers
```

Manual chat entry:

```text
Execute RunMTProtoChat
```

Pick a peer index, type normal text to send, and type `/quit` to exit. User
peers, basic groups and channels/supergroups are handled through the same text
mode when the peer cache contains the required access data. Chat mode auto-reads
incoming peer messages every 2 seconds while waiting for input, even while a
line is being typed.

Quick chat switching: `F1..F10` jump straight to chat 1..10 from the list
(Shift+F1..F10 reach chats 11..20), `Tab` jumps back to the previous chat
(`/swap` does the same), and typing a bare chat number followed by Enter
switches to it. `/peers` shows the list with the open chat marked `*`.
`/search text` finds a cached chat, `/add name` searches Telegram, `/remove n`
drops an entry. `/watch <seconds>` changes the auto-read interval and
`/watch off` disables it. Up/Down arrows recall typed lines.

The chat runs in a full-screen console layout: status bar with the open
chat's name, scrolling transcript, fixed input line (`--ui-tui off` returns
to the classic linear flow; avoid resizing the window mid-chat). The screen
is black high-contrast: contact names bold in the accent colour, your own
name bold white, system lines dim. Message lines are
prefixed with their [HH:MM] time and a separator marks day changes. Messages
arriving in other chats (users, groups, channels) appear as highlighted
`[n] Name: preview` lines: press Fn or type the number to jump there
(`/bell off` disables the console flash). Multi-line messages
keep their line breaks; emoji are rendered as text emoticons (`:)` `<3` `(y)`)
because Amiga consoles cannot draw emoji glyphs. `/color` (or `/color off`)
toggles colours at runtime; `--ui-theme plain` keeps the normal window
colours; `--ui-charset utf8` passes emoji through unchanged for UTF-8
terminals (ssh). Text size follows the system console font preferences
(AmigaOS 4: Prefs -> Fonts; AmigaOS 3: Prefs -> Font; MorphOS/AROS: font
preferences).

If a command reports `auth-dc-mismatch`, run `Execute RunMTProtoInspectAuth`
and use the `dc_id` shown there with the matching Telegram endpoint. The live
AmigaOS 3.x validation for this package used:

```text
Execute RunMTProtoChat 149.154.167.91 443 4 telegram-api.txt telegram-auth.bin telegram-peers.txt telegram-test
```

Delete local account state with:

```text
telegram-test --mtproto-auth-forget telegram-auth.bin phone-code-hash.txt
```
