<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# MTProto Bootstrap

This branch starts the MTProto work as an isolated experimental path. It does
not replace the stable Bot API tester and does not perform Telegram user login
yet.

## Scope

Current MTProto code is offline-only:

- TL little-endian primitive writer/reader;
- TL `bytes`/`string` length and 4-byte padding handling;
- `req_pq_multi` constructor serialization sample;
- portable SHA-1 and SHA-256 primitives with known-answer tests;
- local MTProto session-state save/load skeleton.

Run:

```text
telegram-test --mtproto-self-test
```

Expected output:

```text
mtproto tl self-test: ok
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
- <https://core.telegram.org/mtproto/auth_key>
- <https://core.telegram.org/mtproto/samples-auth_key>

Important constraints for this codebase:

- MTProto 2.0 is the target protocol version.
- Small integer-like TL values are serialized little-endian.
- Large number byte strings used by auth-key creation are big-endian payloads.
- SHA-1 is still needed during authorization-key creation.
- SHA-256 is required by MTProto 2.0 encrypted message handling.

## Next Steps

Next MTProto work should stay behind explicit self-tests:

1. add `req_pq_multi` plain-message envelope construction;
2. add TCP abridged/intermediate transport framing tests;
3. add Telegram DC endpoint configuration without user credentials;
4. add factorization and RSA fingerprint plumbing for auth-key creation;
5. only then attempt a supervised live `req_pq_multi` exchange.
