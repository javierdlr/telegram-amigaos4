<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# TLS Certificate Validation Plan

Current TLS backends encrypt traffic and use SNI where supported. OpenSSL-based
backends can now request certificate-chain and hostname validation with
`--tls-verify`, optionally paired with `--tls-ca-file` or `--tls-ca-path`.
AmiSSL validation still needs backend work. Connections without validation are
acceptable only for supervised testing with disposable bot tokens.

## Required Behavior

Before Telegram Amiga can be described as safe for normal use, each TLS backend
must:

- load a platform-appropriate CA trust store;
- verify the server certificate chain;
- verify the hostname against the certificate;
- fail closed when verification fails;
- print a clear diagnostic without exposing tokens.

## Platform Plan

MorphOS/OpenSSL:

- use OpenSSL verification APIs;
- load either the system CA path when available or a documented CA bundle;
- enable hostname verification with the OpenSSL API available on the target.

AmigaOS 3.x/AmiSSL:

- use AmiSSL/OpenSSL verification APIs available through the selected AmiSSL v5
  API level;
- prefer the system AmiSSL trust store when present;
- document the exact AmiSSL drawer/files needed for testers.

AmigaOS 4.x/AmiSSL:

- follow the same AmiSSL policy as AmigaOS 3.x;
- keep the SDK/build helper independent from private local paths.

AROS:

- plain TCP is implemented through `bsdsocket.library`;
- AROS One i386 alt-abiv0 can link a TLS-enabled build against OpenSSL 1.1.0h
  from the AROS SDK;
- the first supervised HTTPS preflight and Telegram `getMe` tests passed on
  AROS One i386 alt-abiv0;
- `--tls-verify --tls-ca-file DH0:TGTEST/ca-bundle.crt` passed against
  `api.telegram.org` with the YAM CA bundle from the AROS One DVD;
- evaluate AmiSSL/runtime OpenSSL availability separately for 32-bit and
  64-bit AROS;
- document a practical CA-store strategy before enabling validation by default.

## Current User-Facing Status

Run:

```text
telegram-test --telegram-tls-status
```

Expected current output includes:

```text
certificate validation requested: no
```
