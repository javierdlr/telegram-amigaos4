<!--
Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
SPDX-License-Identifier: MIT
-->

# MTProto Bootstrap

This is the developer reference for the MTProto client: the auth-key handshake,
the user-login commands and the saved-session RPCs. The Bot API text path
remains available as a fallback.

## Scope

The MTProto implementation covers the full auth-key handshake and the
user-login/session layer:

- transport: TL little-endian writer/reader, TCP abridged/intermediate framing,
  plain and MTProto 2.0 encrypted envelopes, deterministic `msg_id`,
  `bad_msg_notification` / `bad_server_salt` recovery, best-effort `msgs_ack`;
- handshake: `req_pq_multi` -> `resPQ` (nonce check, `pq` factorization, RSA
  fingerprint selection, built-in production RSA keys) -> `req_DH_params`
  (RSA_PAD + AES-256-IGE) -> `server_DH_params_ok` decrypt + nonce/hash/prime
  validation -> `set_client_DH_params` -> `dh_gen_ok` -> auth-key + initial
  `server_salt` derivation;
- login: `initConnection`/`invokeWithLayer`, `auth.sendCode`/`signIn`/`signUp`,
  SRP 2FA via `account.getPassword` + `auth.checkPassword`, and an interactive
  login wizard;
- session: `help.getConfig`, `users.getUsers(self)`, `messages.getDialogs`
  (with a local peer cache), `messages.getHistory`, `messages.sendMessage`, an
  interactive chat mode, and local session-state save/load;
- crypto: in-tree SHA-1/SHA-256, AES-256-IGE, RSA and big-int modexp, SRP, each
  with known-answer self-tests; auth-key files refuse to save without secure RNG.

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
telegram-test --mtproto-auth-send-code-file <host> <port> <dc-id> <api-file> <phone> <auth-file> <code-hash-file>
telegram-test --mtproto-auth-sign-in <host> <port> <api-id> <auth-file> <phone> <code-hash-file> <code> <dc-id>
telegram-test --mtproto-auth-sign-in-file <host> <port> <api-file> <auth-file> <phone> <code-hash-file> <code> <dc-id>
telegram-test --mtproto-auth-sign-up <host> <port> <api-id> <auth-file> <phone> <code-hash-file> <first-name> <last-name> <dc-id>
telegram-test --mtproto-auth-get-config <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-config-file <host> <port> <api-file> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-password <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-password-file <host> <port> <api-file> <auth-file> <dc-id>
telegram-test --mtproto-auth-check-password <host> <port> <api-id> <auth-file> <dc-id> <password-file>
telegram-test --mtproto-auth-check-password-file <host> <port> <api-file> <auth-file> <dc-id> <password-file>
telegram-test --mtproto-auth-login-wizard-file <host> <port> <dc-id> <api-file> <auth-file> <code-hash-file>
telegram-test --mtproto-auth-status <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-status-file <host> <port> <api-file> <auth-file> <dc-id>
telegram-test --mtproto-auth-inspect <auth-file>
telegram-test --mtproto-auth-check-local-files <api-file> <auth-file> [password-file] [code-hash-file]
telegram-test --mtproto-auth-get-self <host> <port> <api-id> <auth-file> <dc-id>
telegram-test --mtproto-auth-get-dialogs <host> <port> <api-id> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-get-dialogs-file <host> <port> <api-file> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-list-peers-file <host> <port> <api-file> <auth-file> <dc-id> <limit> <peer-cache-file>
telegram-test --mtproto-auth-get-history-self <host> <port> <api-id> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-get-history-self-file <host> <port> <api-file> <auth-file> <dc-id> <limit>
telegram-test --mtproto-auth-get-history-peer-file <host> <port> <api-file> <auth-file> <dc-id> <peer-cache-file> <peer-index> <limit>
telegram-test --mtproto-auth-send-self <host> <port> <api-id> <auth-file> <dc-id> <text>
telegram-test --mtproto-auth-send-peer-file <host> <port> <api-file> <auth-file> <dc-id> <peer-cache-file> <peer-index> <text>
telegram-test --mtproto-chat-file <host> <port> <api-file> <auth-file> <dc-id> <peer-cache-file>
telegram-test --mtproto-auth-forget <auth-file> [code-hash-file]
```

`auth.sendCode` creates and saves a plaintext local auth-key file only after a
successful Telegram response, and only when secure random bytes are available.
Use `--platform-rng-test` before first-login tests on retro targets. It prints
only whether secure RNG is available and never prints random bytes. On
Amiga-like systems this normally requires a TLS/AmiSSL/OpenSSL-enabled build.
`auth.signIn` reuses that file plus the saved `phone_code_hash` and the
human-entered code. If Telegram returns `SESSION_PASSWORD_NEEDED`, put the 2FA
password in a local ignored file such as `telegram-password.txt` and run
`auth.checkPassword`. The password is read from that file, not from argv, and
the command prints only status lines.
`auth.signUp` is available for Test DC numbers that have a validated code hash
but do not yet have a user record.
`auth.login-wizard-file` is the interactive production path: it reads the phone
number, Telegram login code and optional 2FA password from stdin. The login code
and password are not passed through argv; the 2FA password is used in memory and
not written to `telegram-password.txt`.
`auth.list-peers-file` calls `messages.getDialogs` and writes an ignored local
peer cache, typically `telegram-peers.txt`. It always saves dialog peer ids,
top-message ids and unread counts. It also scans returned user/chat constructors
to attach labels and access hashes to matching dialog peers without printing
message text.
`auth.get-history-peer-file` uses that cache to read a history summary for a
cached peer. `auth.send-peer-file` sends a real text message to a cached user,
basic group or channel/supergroup peer; use it only after confirming the peer
index and the account's Telegram permissions.
`mtproto-chat-file` is the first interactive text chat mode for cached peers. It
refreshes the peer cache, asks for a peer index, prints recent text messages
from that peer, then accepts plain text to send. In this mode command
diagnostics are hidden from the chat transcript; `/read`, `/peer`, `/peers` and
`/quit` are the main controls. While waiting for input it auto-reads new
incoming text every 2 seconds; incoming lines use the selected peer label.
`/watch <seconds>` changes
the interval and `/watch off` disables it. Peer refreshes merge with the
existing local cache instead of replacing it destructively.
Some Telegram responses are `gzip_packed`. Builds can enable unpacking with
`TG_ENABLE_GZIP=1` when zlib is available; otherwise these responses remain
explicitly unsupported instead of being silently misparsed.
For Telegram Test DC endpoints, pass either the raw `10000 + dc` value or the
`test:<dc>` shorthand, for example `test:2`.
See [MTPROTO_TEST_DC.md](MTPROTO_TEST_DC.md) for the real Test DC command flow.

To avoid putting `api_hash` in shell history or screenshots, create a local
ignored `telegram-api.txt` file with exactly two non-empty lines:

```text
<api_id>
<api_hash>
```

Use `--mtproto-auth-send-code-file` for the first login step. Later status
checks can use `--mtproto-auth-status-file`, which reads only the `api_id` from
that same file and does not print account details.

The same status check is wrapped by:

```text
scripts/mtproto-login-status.sh <host> <port> telegram-api.txt telegram-auth.bin <dc-id> [program]
```

`messages.getDialogs` currently extracts peer type, peer id, top message id and
unread count from the dialog vector. The peer cache stores matching display
metadata and access hashes from returned users/chats vectors. User and
channel/supergroup sends require an `access_hash`; basic group sends use the
group id directly.

For a serial non-destructive smoke that also checks local files first:

```text
scripts/mtproto-safe-smoke.sh <host> <port> telegram-api.txt telegram-auth.bin <dc-id> [limit] [password-file|-] [program]
```

With no 2FA password file, the optional program path can be passed directly as
the seventh argument; `-` can also be used as an explicit empty password-file
placeholder.

For the complete real-account runbook, use
[MTPROTO_REAL_LOGIN.md](MTPROTO_REAL_LOGIN.md).
For the terse operator checklist, use
[MTPROTO_QUICK_TEST.md](MTPROTO_QUICK_TEST.md).

Minimal real-account validation sequence:

```text
telegram-test --platform-rng-test
telegram-test --mtproto-auth-login-wizard-file <host> <port> <dc-id> telegram-api.txt telegram-auth.bin phone-code-hash.txt
```

On Amiga-style targets, the packaged wrapper is:

```text
Execute RunMTProtoLoginWizard
```

The older step-by-step sequence remains useful for debugging:

```text
telegram-test --mtproto-auth-send-code-file <host> <port> <dc-id> telegram-api.txt <phone> telegram-auth.bin phone-code-hash.txt
telegram-test --mtproto-auth-sign-in-file <host> <port> telegram-api.txt telegram-auth.bin <phone> phone-code-hash.txt <code> <dc-id>
```

If sign-in reports `two-factor-password-required`, create
`telegram-password.txt` locally with only the 2FA password and then run:

```text
telegram-test --mtproto-auth-check-password-file <host> <port> telegram-api.txt telegram-auth.bin <dc-id> telegram-password.txt
telegram-test --mtproto-auth-status-file <host> <port> telegram-api.txt telegram-auth.bin <dc-id>
```

`telegram-api.txt`, `telegram-auth*.bin`, `phone-code-hash*.txt`,
`telegram-password.txt`, `*.api`, `*.auth`, `*.session` and `*.password.txt`
are ignored by Git. Keep them local; do not paste their contents or terminal
screenshots that reveal them.

After sign-in, `help.getConfig` is the first saved-session read-only probe.
`users.getUsers(inputUserSelf)` prints a minimal current-user summary and
confirms that the saved session represents a user identity. `account.getPassword`
parses the current SRP KDF constructor, salt lengths, `g`, prime length, SRP
`B` length and `srp_id`. `auth.checkPassword` fetches those SRP parameters,
derives the SRP proof in memory, sends `auth.checkPassword`, and saves the
updated auth state on success. `auth.status` probes the saved session with
`users.getUsers(inputUserSelf)` but prints only session status, not user
identity details. `auth.inspect` and `auth.check-local-files` are offline
preflight tools for saved auth state and local secret files. `messages.getDialogs` and
`messages.getHistory(inputPeerSelf)` print constructor/count summaries, while
`messages.sendMessage(inputPeerSelf)` sends a cautious first write probe to
Saved Messages. Encrypted RPC responses are acknowledged with best-effort
`msgs_ack` messages before closing the connection. Use `auth.forget` to remove
plaintext local auth-key test files.

Saved authorization files persist `seq_no` and the last client `msg_id`, so
saved-session commands that share one auth file must be run serially. Short
command-line probes can then run back-to-back without reusing stale message
state. The encrypted query helper retries once after `bad_server_salt` and after
recoverable sequence-number `bad_msg_notification` errors.

Run:

```text
telegram-test --mtproto-self-test
```

On slow targets such as 68k AmigaOS, run the non-heavy subset first:

```text
telegram-test --mtproto-self-test-fast
```

Then run the slow RSA/bigint/SRP checks separately when the machine can be left
busy long enough:

```text
telegram-test --mtproto-self-test-heavy
```

Expected output:

```text
mtproto dc self-test: ok
mtproto message-id self-test: ok
mtproto auth self-test: ok
mtproto rsa self-test: ok
mtproto bigint self-test: ok
mtproto tl self-test: ok
mtproto envelope self-test: ok
mtproto encrypted self-test: ok
mtproto transport self-test: ok
mtproto login self-test: ok
mtproto probe self-test: ok
mtproto crypto self-test: ok
mtproto srp self-test: ok
mtproto session self-test: ok
mtproto self-test: ok
```

## Notes

- auth-key files are plaintext local test artifacts and must not be committed;
  delete them with `--mtproto-auth-forget`;
- the Bot API text path stays independent and does not depend on MTProto modules.

## Protocol References

The implementation follows the official Telegram MTProto documentation. Core
references (method/constructor pages are linked from these):

- <https://core.telegram.org/mtproto/description>
- <https://core.telegram.org/mtproto/serialize>
- <https://core.telegram.org/mtproto/transports>
- <https://core.telegram.org/mtproto/auth_key>
- <https://core.telegram.org/mtproto/samples-auth_key>
- <https://core.telegram.org/mtproto/security_guidelines>
- <https://core.telegram.org/schema/mtproto>
- <https://core.telegram.org/api/srp>

Important constraints for this codebase:

- MTProto 2.0 is the target protocol version.
- Small integer-like TL values are serialized little-endian.
- Large number byte strings used by auth-key creation are big-endian payloads.
- SHA-1 is still needed during authorization-key creation.
- SHA-256 is required by MTProto 2.0 encrypted message handling.
- Static DC names are only bootstrap metadata. Full DC options must later come
  from Telegram configuration methods after the auth-key path exists.

## Next Steps

Open MTProto work (keep changes behind the explicit self-tests):

1. better peer filtering/searching for large account dialog lists;
2. broader saved-session validation across platforms and account types;
3. entropy hardening for the in-tree RNG on emulated targets.
