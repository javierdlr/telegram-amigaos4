<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# MTProto Experimental Branch

This branch contains the isolated MTProto bootstrap work for `telegram-amiga`.
The stable Bot API text client remains the usable client path; MTProto code is
not used by Bot API commands.

## Current State

Implemented and covered by offline self-tests:

- TL primitive and byte/string serialization;
- `req_pq_multi` body serialization;
- plain-message envelope with `auth_key_id = 0`;
- abridged and intermediate transport framing;
- portable SHA-1 and SHA-256 known-answer tests;
- local session-state save/load skeleton;
- static bootstrap DC name mapping for the official web endpoint names;
- deterministic client `msg_id` generation rules;
- supervised `req_pq_multi` probe packet builder;
- `resPQ` parser with nonce validation;
- `pq` factorization for 64-bit authorization bootstrap values;
- RSA public-key fingerprint selection against a local known-fingerprint list;
- built-in Telegram production RSA public key material;
- `p_q_inner_data_dc` and `req_DH_params` serialization;
- MTProto RSA_PAD with AES-256-IGE and raw RSA public encryption;
- live `req_DH_params` probe with `server_DH_params_ok` parsing;
- `server_DH_inner_data` AES-IGE decrypt plus nonce/hash validation;
- DH prime/g and public-value range validation against Telegram's known
  authorization prime;
- `client_DH_inner_data` and `set_client_DH_params` serialization;
- final `dh_gen_ok` parsing and non-persistent auth-key derivation;
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
  the Bot API client path;
- explicit `auth.signUp` command for validated test numbers that return
  signup-required;
- saved-session `help.getConfig`, `account.getPassword` and
  `users.getUsers(inputUserSelf)` probes;
- first saved-session `messages.getDialogs`, `messages.getHistory` on
  `inputPeerSelf`, and `messages.sendMessage` to Saved Messages probes;
- best-effort `msgs_ack` for encrypted RPC responses and containers;
- local session-forget command for plaintext auth test files.

Run the offline suite:

```text
telegram-test --mtproto-self-test
```

Expected output:

```text
mtproto dc self-test: ok
mtproto message-id self-test: ok
mtproto auth self-test: ok
mtproto rsa self-test: ok
mtproto tl self-test: ok
mtproto envelope self-test: ok
mtproto encrypted self-test: ok
mtproto transport self-test: ok
mtproto login self-test: ok
mtproto probe self-test: ok
mtproto crypto self-test: ok
mtproto session self-test: ok
mtproto self-test: ok
```

## Live Probe

The branch can perform a supervised `req_pq_multi` probe against a raw MTProto
TCP endpoint, or a supervised full authorization-key bootstrap probe:

```text
telegram-test --mtproto-req-pq-probe <host> <port>
telegram-test --mtproto-req-dh-probe <host> <port> <dc-id>
```

This command does not use Telegram user credentials, bot tokens or saved user
sessions. It opens a TCP connection, sends an abridged-transport plain
`req_pq_multi` message, parses the `resPQ` response, validates the echoed nonce
and factors `pq`. The `req_DH_params` probe additionally sends RSA_PAD
`p_q_inner_data_dc`, parses `server_DH_params_ok`, decrypts
`server_DH_inner_data`, validates the response hash, nonces, DH prime and
generator, sends `set_client_DH_params`, parses `dh_gen_ok`, and derives the
temporary auth key in process memory. It then derives the initial session
metadata and sends one encrypted MTProto `ping`, accepting either a direct
`pong` or a container holding the expected `pong`.

Use this only as a connectivity/protocol-shape check. It is not a login flow
and it does not create or persist an authorization key.

## Experimental User Auth

User-auth commands are explicit and intended for supervised testing only:

```text
telegram-test --mtproto-auth-send-code <host> <port> <dc-id> <api-id> <api-hash> <phone> <auth-file> <code-hash-file>
telegram-test --mtproto-auth-sign-in <host> <port> <api-id> <auth-file> <phone> <code-hash-file> <code> <dc-id>
telegram-test --mtproto-auth-sign-up <host> <port> <api-id> <auth-file> <phone> <code-hash-file> <first-name> <last-name> <dc-id>
telegram-test --mtproto-auth-get-config <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-password <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-self <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-dialogs <host> <port> <api-id> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-get-history-self <host> <port> <api-id> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-send-self <host> <port> <api-id> <auth-file> <dc-id> <text>
telegram-test --mtproto-auth-forget <auth-file> [code-hash-file]
```

`auth.sendCode` creates a fresh MTProto auth key, sends the login-code request,
then saves the plaintext local auth-key file and the `phone_code_hash` file only
after Telegram accepts the request. Saving is refused when the platform backend
cannot provide secure random bytes. `auth.signIn` loads those files and sends
the human-entered code.

For Telegram Test DC endpoints, pass either `10000 + dc` as the `<dc-id>` or use
the `test:<dc>` shorthand, for example `test:2`.

These commands do not implement SRP password login yet. If Telegram returns
`SESSION_PASSWORD_NEEDED`, the account requires 2FA support that is still
pending. `account.getPassword` is present only to confirm whether SRP metadata
is available; it does not compute or submit the password proof yet. After a
successful login, `users.getUsers(inputUserSelf)` prints a minimal current-user
summary without storing a peer database. `messages.getDialogs` and
`messages.getHistory(inputPeerSelf)` currently print constructor/count summaries
only. `messages.sendMessage(inputPeerSelf)` sends to Saved Messages and is the
first cautious write probe.

For account-login development without a real phone number, use Telegram Test DC
numbers and the procedure in [docs/MTPROTO_TEST_DC.md](docs/MTPROTO_TEST_DC.md).

The auth file contains a plaintext MTProto auth key. Keep it local, use only
disposable test accounts at this stage, and delete it with
`--mtproto-auth-forget` when a test is done.

## Branch Rules

- Keep MTProto behind explicit commands and self-tests until auth-key creation
  is complete.
- Keep Bot API regressions green after every MTProto change.
- Do not commit local credentials, Telegram tokens, AI diaries, transcripts or
  temporary target-access notes.
- Distill durable decisions into commit messages and curated documentation.

## Next Work

The next development loop should add:

- real-account validation with a disposable Telegram API id/hash and a test
  phone number;
- full SRP password proof generation before treating 2FA accounts as usable;
- real-account validation of `users.getUsers(inputUserSelf)` after sign-in;
- full peer parsing for dialogs, users/chats and selectable message history;
- target-side validation of saved-session commands on AmigaOS3, MorphOS and
  AROS.

## References

- <https://core.telegram.org/mtproto>
- <https://core.telegram.org/mtproto/description>
- <https://core.telegram.org/mtproto/serialize>
- <https://core.telegram.org/mtproto/transports>
- <https://core.telegram.org/mtproto/samples-auth_key>
- <https://core.telegram.org/schema/mtproto>
- <https://core.telegram.org/mtproto/service_messages>
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
