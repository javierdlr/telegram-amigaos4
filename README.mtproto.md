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
- supervised `req_pq_multi` probe packet builder.

Run the offline suite:

```text
telegram-test --mtproto-self-test
```

Expected output:

```text
mtproto dc self-test: ok
mtproto message-id self-test: ok
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
TCP endpoint:

```text
telegram-test --mtproto-req-pq-probe <host> <port>
```

This command does not use Telegram user credentials, bot tokens or saved user
sessions. It opens a TCP connection, sends an abridged-transport plain
`req_pq_multi` message and checks whether the first response constructor is
`resPQ`.

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

- `resPQ` parsing with nonce validation;
- `pq` factorization tests;
- public RSA fingerprint matching;
- `req_DH_params` construction;
- then a supervised auth-key handshake.

## References

- <https://core.telegram.org/mtproto>
- <https://core.telegram.org/mtproto/description>
- <https://core.telegram.org/mtproto/serialize>
- <https://core.telegram.org/mtproto/transports>
- <https://core.telegram.org/mtproto/samples-auth_key>
