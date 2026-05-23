# MTProto Quick Test

This is the shortest real-account MTProto validation path.

Do not share screenshots that show `telegram-api.txt`, `telegram-auth.bin`,
`phone-code-hash.txt`, `telegram-password.txt`, phone numbers, login codes or
Telegram account details.

## Files

Put these files next to `telegram-test`:

```text
telegram-test
telegram-api.txt
```

`telegram-api.txt` must contain:

```text
<api_id>
<api_hash>
```

Use these default output files:

```text
telegram-auth.bin
phone-code-hash.txt
telegram-password.txt
```

## Production DC 2

```text
HOST=149.154.167.50
PORT=443
DC_ID=2
```

On Amiga shells without variables, replace `$HOST`, `$PORT` and `$DC_ID` with
those literal values.

## Login

Easiest package flow on Amiga-style shells:

```text
Execute RunMTProtoStart
```

If no saved login exists, this runs the interactive login wizard first. After
login it uses the saved auth DC and starts chat.

Preferred interactive login:

```text
scripts/mtproto-login-wizard.sh $HOST $PORT $DC_ID telegram-api.txt telegram-auth.bin phone-code-hash.txt
```

On Amiga shells, use:

```text
Execute RunMTProtoLoginWizard
```

The wizard prompts for the phone number, Telegram login code and optional 2FA
password. The login code and password are not passed as command-line arguments.

For debugging, the older split flow is still available. Send code:

```text
scripts/mtproto-send-code.sh $HOST $PORT $DC_ID telegram-api.txt <phone> telegram-auth.bin phone-code-hash.txt
```

Complete sign-in:

```text
scripts/mtproto-sign-in.sh $HOST $PORT telegram-api.txt telegram-auth.bin <phone> phone-code-hash.txt <code> $DC_ID
```

On Amiga shells, use:

```text
Execute RunMTProtoSendCode <phone>
Execute RunMTProtoSignIn <phone> <code>
```

If Telegram requires 2FA, put only the password in `telegram-password.txt` and
run:

```text
scripts/mtproto-check-password.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-password.txt
```

On Amiga shells, use:

```text
Execute RunMTProtoCheckPassword
```

## Smoke

After sign-in:

```text
scripts/mtproto-login-smoke.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10 telegram-password.txt
```

On Amiga shells, use:

```text
Execute RunMTProtoLoginSmoke
```

Expected high-level result:

```text
mtproto local-files: ok
mtproto auth.inspect: file valid
mtproto auth.status: session valid
mtproto help.getConfig: ok
mtproto messages.getDialogs: ok
mtproto messages.getHistory(self): ok
```

The smoke test is read-only and does not print message text, contact names or
usernames.

`messages.getDialogs` may also print `peer` lines with peer type, id, top
message id and unread count. These are non-message-content handles for the next
client step: selecting a person, group or channel.

To save those handles locally:

```text
telegram-test --mtproto-auth-list-peers-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 20 telegram-peers.txt
```

`telegram-peers.txt` is ignored by Git. The command is read-only; it may save
user/group/channel labels and access hashes for matching dialog peers, but does
not print or persist message text.

To read a cached peer summary without printing message text:

```text
telegram-test --mtproto-auth-get-history-peer-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-peers.txt 1 1
```

To send to a cached user, basic group or channel/supergroup peer, first verify
the peer index in `telegram-peers.txt`, then run:

```text
telegram-test --mtproto-auth-send-peer-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-peers.txt 1 "Hello from Amiga"
```

For the first interactive cached-peer chat mode:

```text
telegram-test --mtproto-chat-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-peers.txt
```

Choose a peer index and type text to send. Chat mode auto-reads new incoming
peer text every 2 seconds while waiting for keyboard input; use `/watch <seconds>`
to change the interval or `/watch off` to disable it. Use `/read` to
print recent cached-user message text, `/peer` to pick another peer, `/peers`
to reload the list and `/quit` to exit. Chat mode hides MTProto diagnostics
during normal operation and prints chat lines with the selected peer
label. Peer refreshes merge with the existing local cache so previously known
peers are not dropped just because Telegram omits them from the latest dialog
page.

## Cleanup

```text
telegram-test --mtproto-auth-forget telegram-auth.bin phone-code-hash.txt
delete telegram-api.txt
delete telegram-password.txt
```

Use `rm -f` instead of `delete` on Unix-like shells.
