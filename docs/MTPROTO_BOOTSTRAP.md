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
  `rpc_result`, `rpc_error`, `bad_msg_notification` and `bad_server_salt`
  serialization/parsing scaffolding;
- explicit live `auth.sendCode` and `auth.signIn` commands, still isolated from
  the Bot API path;
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
telegram-test --mtproto-auth-sign-in <host> <port> <api-id> <auth-file> <phone> <code-hash-file> <code> <dc-id>
```

`auth.sendCode` creates and saves a plaintext local auth-key file only after a
successful Telegram response, and only when secure random bytes are available.
`auth.signIn` reuses that file plus the saved `phone_code_hash` and the
human-entered code. `SESSION_PASSWORD_NEEDED` is reported as unsupported until
SRP password login is implemented.

Run:

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

## Design Boundary

MTProto remains separate from the Bot API path:

- no Bot API command depends on MTProto modules;
- live MTProto user-auth commands are explicit and separate from Bot API
  commands;
- auth-key files are plaintext local test artifacts and must not be committed;
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

1. validate the auth commands with a disposable Telegram API id/hash and a test
   phone number;
2. add SRP password support before treating 2FA accounts as usable;
3. improve session-file UX and warnings for plaintext local auth-key storage;
4. add the first authenticated read-only user RPC after sign-in;
5. keep Bot API and MTProto user login commands separate until login,
   encrypted RPC parsing and session persistence are covered by tests.
