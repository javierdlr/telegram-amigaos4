<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# AROS Community Test Request

Hello,

we have a new Telegram Amiga pre-alpha tester for AROS One i386 alt-abiv0.
This is not a complete Telegram client yet: it is a technical Bot API based
tester used to verify networking, TLS, JSON parsing, message polling and
controlled replies on Amiga-like systems.

The most useful tests are:

```text
telegram-test --help
telegram-test --telegram-json-self-test
telegram-test --telegram-get-updates-self-test
telegram-test --telegram-read-once-state-self-test
telegram-test --telegram-inbox-self-test
telegram-test --telegram-echo-once-self-test
telegram-test --telegram-send-message-self-test
telegram-test --telegram-client-self-test
telegram-test --telegram-tls-status
telegram-test --net-test example.com 80
telegram-test --http-test example.com 80 /
```

For TLS-enabled builds, without using a Telegram token:

```text
telegram-test --https-test api.telegram.org 443 /
telegram-test --telegram-preflight
```

If you have a CA bundle available, certificate validation can be tested with:

```text
telegram-test --tls-verify --tls-ca-file ca-bundle.crt --https-test api.telegram.org 443 /
telegram-test --tls-verify --tls-ca-file ca-bundle.crt --telegram-preflight
```

For live Bot API tests, please use only a disposable test bot token created
with `@BotFather`, saved as `telegram-token.txt` in the same drawer as
`telegram-test`. Do not publish tokens or screenshots that show tokens.

Useful live commands:

```text
telegram-test --telegram-getme-default
telegram-test --telegram-client-default
telegram-test --telegram-client-console
telegram-test --telegram-chats-default
telegram-test --telegram-reply-default 1 "Hello from AROS"
```

Please report:

- AROS distribution and version;
- 32-bit or 64-bit;
- whether this is hosted, native, emulated or virtualized;
- compiler name/version if you built from source;
- whether OpenSSL or AmiSSL is available;
- whether `--tls-verify` was tested and which CA bundle/path was used;
- output of the offline, TCP/HTTP, HTTPS and Bot API commands;
- any crash, freeze, requester or unusual delay.

Thank you for helping test Telegram Amiga on AROS.
