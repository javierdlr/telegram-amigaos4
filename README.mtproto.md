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
- MTProto RSA_PAD with AES-256-IGE and raw RSA public encryption.
- live `req_DH_params` probe with `server_DH_params_ok` parsing;
- `server_DH_inner_data` AES-IGE decrypt plus nonce/hash validation.

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
mtproto transport self-test: ok
mtproto probe self-test: ok
mtproto crypto self-test: ok
mtproto session self-test: ok
mtproto self-test: ok
```

## Live Probe

The branch can perform a supervised `req_pq_multi` probe against a raw MTProto
TCP endpoint, or a two-step `req_pq_multi` plus `req_DH_params` probe:

```text
telegram-test --mtproto-req-pq-probe <host> <port>
telegram-test --mtproto-req-dh-probe <host> <port> <dc-id>
```

This command does not use Telegram user credentials, bot tokens or saved user
sessions. It opens a TCP connection, sends an abridged-transport plain
`req_pq_multi` message, parses the `resPQ` response, validates the echoed nonce
and factors `pq`. The `req_DH_params` probe additionally sends RSA_PAD
`p_q_inner_data_dc`, parses `server_DH_params_ok`, decrypts
`server_DH_inner_data` and validates the response hash and nonces.

Use this only as a connectivity/protocol-shape check. It is not a login flow
and it does not create or persist an authorization key.

## Branch Rules

- Keep MTProto behind explicit commands and self-tests until auth-key creation
  is complete.
- Keep Bot API regressions green after every MTProto change.
- Do not commit local credentials, Telegram tokens, AI diaries, transcripts or
  temporary target-access notes.
- Distill durable decisions into commit messages and curated documentation.

## Next Work

The next development loop should add:

- DH prime/g checks and known-prime cache;
- `client_DH_inner_data` and `set_client_DH_params`;
- final auth-key derivation and non-persistent supervised handshake check.

## References

- <https://core.telegram.org/mtproto>
- <https://core.telegram.org/mtproto/description>
- <https://core.telegram.org/mtproto/serialize>
- <https://core.telegram.org/mtproto/transports>
- <https://core.telegram.org/mtproto/samples-auth_key>
