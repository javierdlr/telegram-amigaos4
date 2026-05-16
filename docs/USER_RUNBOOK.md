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
