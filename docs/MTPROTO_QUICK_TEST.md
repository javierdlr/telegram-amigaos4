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

Send code:

```text
scripts/mtproto-send-code.sh $HOST $PORT $DC_ID telegram-api.txt <phone> telegram-auth.bin phone-code-hash.txt
```

On Amiga shells, use:

```text
Execute RunMTProtoSendCode <phone>
```

Complete sign-in:

```text
scripts/mtproto-sign-in.sh $HOST $PORT telegram-api.txt telegram-auth.bin <phone> phone-code-hash.txt <code> $DC_ID
```

On Amiga shells, use:

```text
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

## Cleanup

```text
telegram-test --mtproto-auth-forget telegram-auth.bin phone-code-hash.txt
delete telegram-api.txt
delete telegram-password.txt
```

Use `rm -f` instead of `delete` on Unix-like shells.
