<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# TLS Certificate Validation

Current TLS backends encrypt traffic and use SNI where supported. OpenSSL-based
AROS/MorphOS and AmiSSL-based AmigaOS 3.x/4.x backends can now request
certificate-chain and hostname validation with `--tls-verify`, optionally
paired with `--tls-ca-file` or `--tls-ca-path`. AROS, MorphOS, AmigaOS 3.x and
AmigaOS 4.x have passed live CA-bundle validation smoke tests. Connections
without validation are acceptable only for supervised testing with disposable
bot tokens.

## How validation works

With `--tls-verify`, each TLS backend loads a platform-appropriate CA trust
store, verifies the server certificate chain and hostname, fails closed on
failure, and prints a clear diagnostic without exposing tokens.

## Per-platform status

All four platforms have passed a supervised live CA-bundle validation smoke test
against `api.telegram.org`:

- MorphOS / AROS (OpenSSL): use the OpenSSL verification APIs with the system CA
  path or a documented CA bundle. AROS One i386 alt-abiv0 links OpenSSL 1.1.0h
  from the AROS SDK and passed with
  `--tls-verify --tls-ca-file DH0:TGTEST/ca-bundle.crt` (YAM CA bundle from the
  AROS One DVD).
- AmigaOS 3.x / 4.x (AmiSSL v5): support explicit `--tls-ca-file`/`--tls-ca-path`
  and default verify paths; AmigaOS 4.x needs a correct system date for validity
  checks.

Validation is opt-in (`--tls-verify`); a practical default CA-store strategy is
still to be documented before enabling it by default.

## Current User-Facing Status

Run:

```text
TelegramAmiga --telegram-tls-status
```

Expected current output includes:

```text
certificate validation requested: no
```
