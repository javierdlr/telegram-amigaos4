# MTProto Test DC Login

Telegram provides Test DC accounts for validating user-authorization flows
without a real phone number. Use this only against Telegram Test DC endpoints,
not production DCs.

## Inputs

Get these values from `https://my.telegram.org` under API development tools:

- `api_id`
- `api_hash`
- Test DC host/IP and port
- Test DC id, usually `1`, `2` or `3`

Do not commit these values. Do not reuse a production auth file with a Test DC.

## Test Number

For Test DC `X`, use a phone number in this shape:

```text
99966XYYYY
```

`YYYY` is any four-digit suffix. The login code is the DC number repeated five
times:

```text
XXXXX
```

For Test DC `2`, a valid example shape is:

```text
phone: 9996621234
code:  22222
```

## Flow

Use local temporary files for the saved auth key and phone-code hash:

```text
telegram-test --mtproto-auth-send-code <test-host> <test-port> <dc-id> <api-id> <api-hash> <phone> <auth-file> <code-hash-file>
telegram-test --mtproto-auth-sign-in <test-host> <test-port> <api-id> <auth-file> <phone> <code-hash-file> <code> <dc-id>
```

If `auth.signIn` returns signup-required, register the validated test number:

```text
telegram-test --mtproto-auth-sign-up <test-host> <test-port> <api-id> <auth-file> <phone> <code-hash-file> Amiga Test <dc-id>
```

Then validate the saved session:

```text
telegram-test --mtproto-auth-get-config <test-host> <test-port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-self <test-host> <test-port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-dialogs <test-host> <test-port> <api-id> <auth-file> <dc-id> 20
telegram-test --mtproto-auth-get-history-self <test-host> <test-port> <api-id> <auth-file> <dc-id> 20
telegram-test --mtproto-auth-send-self <test-host> <test-port> <api-id> <auth-file> <dc-id> "hello from amiga"
```

Clean up local plaintext auth artifacts when done:

```text
telegram-test --mtproto-auth-forget <auth-file> <code-hash-file>
```

## Notes

- Test DC data is disposable and may be wiped by Telegram.
- If a suffix hits flood limits, use another `YYYY`.
- Production validation should happen only after the Test DC flow is green.
- Accounts with real 2FA still require SRP password proof support.

References:

- <https://core.telegram.org/api/auth>
- <https://core.telegram.org/method/auth.signUp>
