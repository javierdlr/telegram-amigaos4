<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# TLS Certificate Validation Plan

Current TLS backends encrypt traffic and use SNI where supported, but they do
not validate the server certificate chain or hostname yet. This is acceptable
only for supervised testing with disposable bot tokens.

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

- implement plain TCP first;
- evaluate AmiSSL availability separately for 32-bit and 64-bit AROS;
- add HTTPS only after a concrete CA-store strategy exists.

## Current User-Facing Status

Run:

```text
telegram-test --telegram-tls-status
```

Expected current output includes:

```text
certificate validation: disabled
```
