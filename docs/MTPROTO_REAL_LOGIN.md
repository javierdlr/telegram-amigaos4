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
telegram-test --mtproto-auth-send-code-file $HOST $PORT $DC_ID telegram-api.txt <phone> telegram-auth.bin phone-code-hash.txt
```

Complete login with the code received from Telegram:

```text
telegram-test --mtproto-auth-sign-in $HOST $PORT <api_id> telegram-auth.bin <phone> phone-code-hash.txt <code> $DC_ID
```

If Telegram reports that 2FA is required, put only the account password in
`telegram-password.txt`, then run:

```text
telegram-test --mtproto-auth-check-password $HOST $PORT <api_id> telegram-auth.bin $DC_ID telegram-password.txt
```

Validate without printing account identity:

```text
telegram-test --mtproto-auth-status-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
telegram-test --mtproto-auth-get-config-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
telegram-test --mtproto-auth-get-dialogs-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
telegram-test --mtproto-auth-get-history-self-file $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
```

Wrapper equivalents:

```text
scripts/mtproto-send-code.sh $HOST $PORT $DC_ID telegram-api.txt <phone> telegram-auth.bin phone-code-hash.txt
scripts/mtproto-sign-in.sh $HOST $PORT <api_id> telegram-auth.bin <phone> phone-code-hash.txt <code> $DC_ID
scripts/mtproto-check-password.sh $HOST $PORT <api_id> telegram-auth.bin $DC_ID telegram-password.txt
scripts/mtproto-login-status.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
scripts/mtproto-get-config.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID
scripts/mtproto-get-dialogs.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
scripts/mtproto-get-history-self.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
scripts/mtproto-readonly-smoke.sh $HOST $PORT telegram-api.txt telegram-auth.bin $DC_ID 10
```

The read-only smoke wrapper runs status, config, dialog summary and Saved
Messages history summary. It is the preferred first validation after sign-in
because it does not print contact names, usernames or message text.

## Cleanup

To remove local login state:

```text
telegram-test --mtproto-auth-forget telegram-auth.bin phone-code-hash.txt
rm -f telegram-api.txt telegram-password.txt
```

Do not commit or push any local login files.
