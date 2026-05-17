<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# MTProto Bootstrap

This branch starts the MTProto work as an isolated experimental path. It does
not replace the stable Bot API tester and does not perform Telegram user login
yet.

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
- platform RNG plumbing for probes, with no persistent-key save path yet;
- portable SHA-1 and SHA-256 primitives with known-answer tests;
- local MTProto session-state save/load skeleton.

The optional `--mtproto-req-pq-probe <host> <port>` and
`--mtproto-req-dh-probe <host> <port> <dc-id>` commands are supervised
connectivity checks only. They do not perform user login and they do not create
or persist an authorization key. The DH probe reaches `dh_gen_ok`, derives the
auth key in process memory, validates Telegram's final nonce hash, sends one
encrypted MTProto `ping`, validates the encrypted `pong`, and then discards the
key.

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
mtproto probe self-test: ok
mtproto crypto self-test: ok
mtproto session self-test: ok
mtproto self-test: ok
```

## Design Boundary

MTProto remains separate from the Bot API path:

- no Bot API command depends on MTProto modules;
- no MTProto code performs live auth or stores real Telegram user sessions yet;
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

1. add curated storage for the auth key itself, with a strict no-save path when
   platform RNG is unavailable;
2. add `auth.sendCode`/`auth.signIn` scaffolding behind explicit API-id/hash
   input;
3. add SRP password support before treating 2FA accounts as usable;
4. keep Bot API and MTProto user login commands separate until login,
   encrypted RPC parsing and session persistence are covered by tests.
