<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# MTProto Bootstrap

This branch starts the MTProto work as an isolated experimental path. It does
not replace the stable Bot API tester. User-login commands now exist, but they
are explicit supervised probes rather than a general-purpose client.

## Scope

Current MTProto code is offline by default:

- TL little-endian primitive writer/reader;
- TL `bytes`/`string` length and 4-byte padding handling;
- `req_pq_multi` constructor serialization sample;
- MTProto plain-message envelope sample with `auth_key_id = 0`;
- TCP abridged and intermediate transport init plus packet framing samples;
- static bootstrap DC name mapping based on Telegram web endpoint names;
- deterministic client `msg_id` generation rules;
- supervised `req_pq_multi` probe packet builder;
- `resPQ` parser with nonce validation;
- `pq` factorization tests;
- RSA public-key fingerprint selection against a known-fingerprint list;
- built-in Telegram production RSA public key material;
- `p_q_inner_data_dc` and `req_DH_params` serialization;
- MTProto RSA_PAD with AES-256-IGE and raw RSA public encryption;
- live `req_DH_params` probe with `server_DH_params_ok` parsing;
- `server_DH_inner_data` AES-IGE decrypt plus nonce/hash validation;
- DH prime/g and public-value range validation against Telegram's known
  authorization prime;
- `client_DH_inner_data` and `set_client_DH_params` serialization;
- `dh_gen_ok` parsing and non-persistent auth-key derivation;
- `auth_key_id` and initial `server_salt` metadata derivation;
- MTProto 2.0 encrypted-message framing and response decryption;
- supervised encrypted `ping`/`pong` probe after auth-key creation;
- platform RNG plumbing for probes;
- curated auth-key file save/load helpers that refuse to save when secure RNG
  is unavailable;
- `initConnection`, `invokeWithLayer`, `auth.sendCode`, `auth.signIn`,
  `auth.signUp`,
  `rpc_result`, `rpc_error`, `bad_msg_notification` and `bad_server_salt`
  serialization/parsing scaffolding;
- explicit live `auth.sendCode` and `auth.signIn` commands, still isolated from
  the Bot API path;
- explicit `auth.signUp` command for validated test numbers that return
  signup-required;
- saved-session `help.getConfig`, `account.getPassword` and
  `users.getUsers(inputUserSelf)` probes;
- first saved-session `messages.getDialogs`, `messages.getHistory` on
  `inputPeerSelf`, and `messages.sendMessage` to Saved Messages probes;
- best-effort `msgs_ack` for encrypted RPC responses and containers;
- local session-forget command for plaintext auth test files;
- portable SHA-1 and SHA-256 primitives with known-answer tests;
- local MTProto session-state save/load skeleton.

The optional `--mtproto-req-pq-probe <host> <port>` and
`--mtproto-req-dh-probe <host> <port> <dc-id>` commands are supervised
connectivity checks only. They do not perform user login and they do not create
or persist an authorization key. The DH probe reaches `dh_gen_ok`, derives the
auth key in process memory, validates Telegram's final nonce hash, sends one
encrypted MTProto `ping`, validates the encrypted `pong`, and then discards the
key.

The optional user-auth commands are:

```text
telegram-test --mtproto-auth-send-code <host> <port> <dc-id> <api-id> <api-hash> <phone> <auth-file> <code-hash-file>
telegram-test --mtproto-auth-send-code-file <host> <port> <dc-id> <api-file> <phone> <auth-file> <code-hash-file>
telegram-test --mtproto-auth-sign-in <host> <port> <api-id> <auth-file> <phone> <code-hash-file> <code> <dc-id>
telegram-test --mtproto-auth-sign-in-file <host> <port> <api-file> <auth-file> <phone> <code-hash-file> <code> <dc-id>
telegram-test --mtproto-auth-sign-up <host> <port> <api-id> <auth-file> <phone> <code-hash-file> <first-name> <last-name> <dc-id>
telegram-test --mtproto-auth-get-config <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-config-file <host> <port> <api-file> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-password <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-password-file <host> <port> <api-file> <auth-file> <dc-id>
telegram-test --mtproto-auth-check-password <host> <port> <api-id> <auth-file> <dc-id> <password-file>
telegram-test --mtproto-auth-check-password-file <host> <port> <api-file> <auth-file> <dc-id> <password-file>
telegram-test --mtproto-auth-status <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-status-file <host> <port> <api-file> <auth-file> <dc-id>
telegram-test --mtproto-auth-inspect <auth-file>
telegram-test --mtproto-auth-check-local-files <api-file> <auth-file> [password-file] [code-hash-file]
telegram-test --mtproto-auth-get-self <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-dialogs <host> <port> <api-id> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-get-dialogs-file <host> <port> <api-file> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-get-history-self <host> <port> <api-id> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-get-history-self-file <host> <port> <api-file> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-send-self <host> <port> <api-id> <auth-file> <dc-id> <text>
telegram-test --mtproto-auth-forget <auth-file> [code-hash-file]
```

`auth.sendCode` creates and saves a plaintext local auth-key file only after a
successful Telegram response, and only when secure random bytes are available.
`auth.signIn` reuses that file plus the saved `phone_code_hash` and the
human-entered code. If Telegram returns `SESSION_PASSWORD_NEEDED`, put the 2FA
password in a local ignored file such as `telegram-password.txt` and run
`auth.checkPassword`. The password is read from that file, not from argv, and
the command prints only status lines.
`auth.signUp` is available for Test DC numbers that have a validated code hash
but do not yet have a user record.
Some Telegram responses are `gzip_packed`. Builds can enable unpacking with
`TG_ENABLE_GZIP=1` when zlib is available; otherwise these responses remain
explicitly unsupported instead of being silently misparsed.
For Telegram Test DC endpoints, pass either the raw `10000 + dc` value or the
`test:<dc>` shorthand, for example `test:2`.
See [MTPROTO_TEST_DC.md](MTPROTO_TEST_DC.md) for the real Test DC command flow.

To avoid putting `api_hash` in shell history or screenshots, create a local
ignored `telegram-api.txt` file with exactly two non-empty lines:

```text
<api_id>
<api_hash>
```

Use `--mtproto-auth-send-code-file` for the first login step. Later status
checks can use `--mtproto-auth-status-file`, which reads only the `api_id` from
that same file and does not print account details.

The same status check is wrapped by:

```text
scripts/mtproto-login-status.sh <host> <port> telegram-api.txt telegram-auth.bin <dc-id> [program]
```

For a serial non-destructive smoke that also checks local files first:

```text
scripts/mtproto-safe-smoke.sh <host> <port> telegram-api.txt telegram-auth.bin <dc-id> [limit] [password-file|-] [program]
```

With no 2FA password file, the optional program path can be passed directly as
the seventh argument; `-` can also be used as an explicit empty password-file
placeholder.

For the complete real-account runbook, use
[MTPROTO_REAL_LOGIN.md](MTPROTO_REAL_LOGIN.md).
For the terse operator checklist, use
[MTPROTO_QUICK_TEST.md](MTPROTO_QUICK_TEST.md).

Minimal real-account validation sequence:

```text
telegram-test --mtproto-auth-send-code-file <host> <port> <dc-id> telegram-api.txt <phone> telegram-auth.bin phone-code-hash.txt
telegram-test --mtproto-auth-sign-in-file <host> <port> telegram-api.txt telegram-auth.bin <phone> phone-code-hash.txt <code> <dc-id>
```

If sign-in reports `two-factor-password-required`, create
`telegram-password.txt` locally with only the 2FA password and then run:

```text
telegram-test --mtproto-auth-check-password-file <host> <port> telegram-api.txt telegram-auth.bin <dc-id> telegram-password.txt
telegram-test --mtproto-auth-status-file <host> <port> telegram-api.txt telegram-auth.bin <dc-id>
```

`telegram-api.txt`, `telegram-auth*.bin`, `phone-code-hash*.txt`,
`telegram-password.txt`, `*.api`, `*.auth`, `*.session` and `*.password.txt`
are ignored by Git. Keep them local; do not paste their contents or terminal
screenshots that reveal them.

After sign-in, `help.getConfig` is the first saved-session read-only probe.
`users.getUsers(inputUserSelf)` prints a minimal current-user summary and
confirms that the saved session represents a user identity. `account.getPassword`
parses the current SRP KDF constructor, salt lengths, `g`, prime length, SRP
`B` length and `srp_id`. `auth.checkPassword` fetches those SRP parameters,
derives the SRP proof in memory, sends `auth.checkPassword`, and saves the
updated auth state on success. `auth.status` probes the saved session with
`users.getUsers(inputUserSelf)` but prints only session status, not user
identity details. `auth.inspect` and `auth.check-local-files` are offline
preflight tools for saved auth state and local secret files. `messages.getDialogs` and
`messages.getHistory(inputPeerSelf)` print constructor/count summaries, while
`messages.sendMessage(inputPeerSelf)` sends a cautious first write probe to
Saved Messages. Encrypted RPC responses are acknowledged with best-effort
`msgs_ack` messages before closing the connection. Use `auth.forget` to remove
plaintext local auth-key test files.

Saved authorization files persist `seq_no` and the last client `msg_id`, so
saved-session commands that share one auth file must be run serially. Short
command-line probes can then run back-to-back without reusing stale message
state. The encrypted query helper retries once after `bad_server_salt` and after
recoverable sequence-number `bad_msg_notification` errors.

Run:

```text
telegram-test --mtproto-self-test
```

On slow targets such as 68k AmigaOS, run the non-heavy subset first:

```text
telegram-test --mtproto-self-test-fast
```

Then run the slow RSA/bigint/SRP checks separately when the machine can be left
busy long enough:

```text
telegram-test --mtproto-self-test-heavy
```

Expected output:

```text
mtproto dc self-test: ok
mtproto message-id self-test: ok
mtproto auth self-test: ok
mtproto rsa self-test: ok
mtproto bigint self-test: ok
mtproto tl self-test: ok
mtproto envelope self-test: ok
mtproto encrypted self-test: ok
mtproto transport self-test: ok
mtproto login self-test: ok
mtproto probe self-test: ok
mtproto crypto self-test: ok
mtproto srp self-test: ok
mtproto session self-test: ok
mtproto self-test: ok
```

## Design Boundary

MTProto remains separate from the Bot API path:

- no Bot API command depends on MTProto modules;
- live MTProto user-auth commands are explicit and separate from Bot API
  commands;
- auth-key files are plaintext local test artifacts and must not be committed;
- local auth-key deletion is explicit through `--mtproto-auth-forget`;
- shared network/TLS/file helpers may be reused only after Bot API regression
  tests stay green;
- target-side validation starts only after offline MTProto self-tests are stable.

## Protocol References

The bootstrap follows the official Telegram MTProto documentation:

- <https://core.telegram.org/mtproto>
- <https://core.telegram.org/mtproto/description>
- <https://core.telegram.org/mtproto/serialize>
- <https://core.telegram.org/mtproto/transports>
- <https://core.telegram.org/mtproto/auth_key>
- <https://core.telegram.org/mtproto/samples-auth_key>
- <https://core.telegram.org/schema/mtproto>
- <https://core.telegram.org/mtproto/service_messages>
- <https://core.telegram.org/mtproto/security_guidelines>
- <https://core.telegram.org/method/auth.sendCode>
- <https://core.telegram.org/method/auth.signIn>
- <https://core.telegram.org/method/auth.signUp>
- <https://core.telegram.org/method/help.getConfig>
- <https://core.telegram.org/method/account.getPassword>
- <https://core.telegram.org/method/users.getUsers>
- <https://core.telegram.org/constructor/inputUserSelf>
- <https://core.telegram.org/method/messages.getDialogs>
- <https://core.telegram.org/method/messages.getHistory>
- <https://core.telegram.org/method/messages.sendMessage>
- <https://core.telegram.org/type/InputPeer>
- <https://core.telegram.org/api/srp>

Important constraints for this codebase:

- MTProto 2.0 is the target protocol version.
- Small integer-like TL values are serialized little-endian.
- Large number byte strings used by auth-key creation are big-endian payloads.
- SHA-1 is still needed during authorization-key creation.
- SHA-256 is required by MTProto 2.0 encrypted message handling.
- Static DC names are only bootstrap metadata. Full DC options must later come
  from Telegram configuration methods after the auth-key path exists.

## Next Steps

Next MTProto work should stay behind explicit self-tests:

1. validate the auth commands on Telegram Test DC with a generated test number;
2. add SRP modular exponentiation and proof assembly from the parsed
   `account.Password` parameters, then wire it into the existing
   `auth.checkPassword` builder;
3. validate `users.getUsers(inputUserSelf)` after sign-in;
4. parse dialogs into selectable peers and message-history text rows;
5. validate saved-session commands on AmigaOS3, MorphOS and AROS;
6. keep Bot API and MTProto user login commands separate until login,
   encrypted RPC parsing and session persistence are covered by tests.
