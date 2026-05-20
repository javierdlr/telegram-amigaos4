# MTProto Real Login Test

This runbook is for a cautious real-account MTProto login test. It keeps
`api_hash`, password, auth key and code hash in local files and avoids printing
account identity unless an explicit identity/debug command is used.

## Local Files

Create `telegram-api.txt` next to `telegram-test`:

```text
<api_id>
<api_hash>
```

The normal local files are:

```text
telegram-api.txt
telegram-auth.bin
phone-code-hash.txt
telegram-password.txt
```

They are ignored by Git. Keep them local and do not include them in tester
packages, screenshots, transcripts or commits.

On systems with Unix-style permissions, prefer:

```text
chmod 600 telegram-api.txt telegram-auth.bin phone-code-hash.txt telegram-password.txt
```

Classic Amiga filesystems may not support Unix permissions; in that case the
tool only validates readability and format.

## Preflight

Before using a saved login state:

```text
telegram-test --mtproto-auth-check-local-files telegram-api.txt telegram-auth.bin telegram-password.txt phone-code-hash.txt
telegram-test --mtproto-auth-inspect telegram-auth.bin
```

The inspect command prints only non-secret state such as DC id, sequence status
and whether the saved auth key matches its stored key id. It does not print the
auth key, session id or account identity.

Wrapper equivalents:

```text
scripts/mtproto-check-local-files.sh telegram-api.txt telegram-auth.bin telegram-password.txt phone-code-hash.txt
scripts/mtproto-inspect-auth.sh telegram-auth.bin
```

## Login Sequence

Use Telegram's Production DC configuration unless you are intentionally testing
against Test DC. For the current Production DC 2 endpoint:

```text
HOST=149.154.167.50
PORT=443
DC_ID=2
```

Start login:

```text
telegram-test --mtproto-auth-login-wizard-file $HOST $PORT $DC_ID telegram-api.txt telegram-auth.bin phone-code-hash.txt
```

The wizard asks for the phone number and then for the Telegram login code. If
Telegram requires 2FA, it asks for the password and uses it only in memory. On
some retro consoles the password input is visible; do not run it while sharing
the screen.

AmigaDOS package wrapper:

```text
Execute RunMTProtoLoginWizard
```

For debugging, the same flow can still be run step by step. Start login:

```text
telegram-test --mtproto-auth-send-code-file $HOST $PORT $DC_ID telegram-api.txt <phone> telegram-auth.bin phone-code-hash.txt
```

Complete login with the code received from Telegram:

```text
telegram-test --mtproto-auth-sign-in-file $HOST $PORT telegram-api.txt telegram-auth.bin <phone> phone-code-hash.txt <code> $DC_ID
```

If Telegram reports that 2FA is required, put only the account password in
`telegram-password.txt`, then run:

```text
telegram-test --mtproto-auth-check-password-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-password.txt
```

Validate without printing account identity:

```text
telegram-test --mtproto-auth-status-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
telegram-test --mtproto-auth-get-config-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
telegram-test --mtproto-auth-get-dialogs-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
telegram-test --mtproto-auth-list-peers-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 20 telegram-peers.txt
telegram-test --mtproto-auth-get-history-peer-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-peers.txt 1 1
telegram-test --mtproto-chat-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-peers.txt
telegram-test --mtproto-auth-get-history-self-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
```

Wrapper equivalents:

```text
scripts/mtproto-send-code.sh $HOST $PORT $DC_ID telegram-api.txt <phone> telegram-auth.bin phone-code-hash.txt
scripts/mtproto-sign-in.sh $HOST $PORT telegram-api.txt telegram-auth.bin <phone> phone-code-hash.txt <code> $DC_ID
scripts/mtproto-login-wizard.sh $HOST $PORT $DC_ID telegram-api.txt telegram-auth.bin phone-code-hash.txt
scripts/mtproto-get-password.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
scripts/mtproto-check-password.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-password.txt
scripts/mtproto-login-status.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
scripts/mtproto-get-config.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
scripts/mtproto-get-dialogs.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
scripts/mtproto-list-peers.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 20 telegram-peers.txt
scripts/mtproto-get-history-peer.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-peers.txt 1 1
scripts/mtproto-chat.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID telegram-peers.txt
scripts/mtproto-get-history-self.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
scripts/mtproto-readonly-smoke.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
scripts/mtproto-login-smoke.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10 telegram-password.txt
scripts/mtproto-safe-smoke.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10 telegram-password.txt
scripts/mtproto-safe-smoke.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10 ./telegram-test
```

The read-only smoke wrapper runs status, config, dialog summary and Saved
Messages history summary. It is the preferred first validation after sign-in
because it does not print contact names, usernames or message text.
The dialog step may print peer type, peer id, top message id and unread count.
`mtproto-list-peers` saves those handles in ignored `telegram-peers.txt`; it may
also include user labels and access hashes for matching dialog peers. Treat that
cache as local private state.
Use `mtproto-get-history-peer` as the first cached-peer validation because it is
read-only and prints only counts. `mtproto-send-peer` sends a real message to
the selected peer index.
Use `mtproto-chat` for the first manual botta-risposta mode. It hides protocol
diagnostics from the chat transcript, prints recent peer text as `me:` or the
selected peer label, auto-reads new incoming text every 5 seconds while waiting
for input, and sends only when a normal text line is entered. Use `/watch <seconds>`
or `/watch off` to tune that receive loop. Peer refreshes merge with the
existing local cache, so known peers should remain available even when Telegram
returns a different dialog page.
The login smoke wrapper first validates local files and inspects the saved auth
state, then runs the same read-only sequence.
The safe smoke wrapper performs local-file checks, inspects the auth file, and
then runs the read-only sequence serially. When no 2FA password file is needed,
the last argument can be the program path directly; use `-` as the password-file
placeholder if both a custom program path and an omitted password file must be
made explicit.

Run saved-session commands serially when they share the same `telegram-auth.bin`.
The auth file persists `seq_no` and the last message id after each request;
parallel commands can race and temporarily confuse response matching.

## Cleanup

To remove local login state:

```text
telegram-test --mtproto-auth-forget telegram-auth.bin phone-code-hash.txt
rm -f telegram-api.txt telegram-password.txt
```

Do not commit or push any local login files.
