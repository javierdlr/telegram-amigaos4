# Telegram Amiga

Author: Michele Dipace <michele.dipace@kaffeine.net>

License: MIT

Telegram Amiga is a non-commercial community project, created as a gift to the
Amiga community and as an exploration of what a modern Telegram client for
Amiga-like systems could become.

Current status: early technical bootstrap, not yet a usable Telegram client.

## Goal

Build, one step at a time, a cross-platform Telegram client for:

- AmigaOS 3.x
- MorphOS
- AmigaOS 4
- AROS

The project favors portable C code, platform-specific backends and incremental
testing on real hardware or real target systems.

## AI-Assisted Development

This project is also developed with the help of AI agents, used as assisted
development tools for code analysis, incremental implementation, refactoring,
documentation and test preparation.

The goal is not to replace the experience of the Amiga community. The goal is to
show that AI agents can become a practical support even for historical or niche
platforms, where time, documentation, hardware and toolchains are often precious
resources.

The workflow uses a Mac as the main development machine. Real or emulated
Amiga-like systems are made reachable on the local network through SSH, for
example with BebboSSH on AmigaOS or MorphOS. This lets the agent prepare code on
the Mac, copy it to the target system, run builds and tests, and report concrete
results instead of working only in theory.

This repository is also a small public experiment: can collaboration between
people, real Amiga hardware and AI agents help produce useful new software for
the community?

## Structure

Minimal bootstrap:

- `core/`: portable logic
- `include/`: internal public interfaces
- `platforms/*/`: platform-specific adapters
- `src/main.c`: thin entry point

Initial core modules:

- `tg_config`: minimal command-line argument parsing
- `tg_log`: portable logging delegated to the platform layer
- `tg_http`: minimal HTTP/1.0 over `tg_net`, plus response parsing
- `tg_net`: portable TCP API with an initial MorphOS implementation
- `tg_tls`/`tg_https`: minimal TLS/HTTPS with an initial MorphOS OpenSSL backend

TLS note: the initial MorphOS backend uses OpenSSL/AmiSSL with SNI, but
certificate validation is not enabled yet. This is enough for connectivity
tests, not yet for secure use.

Initial targets:

- MorphOS: active and verified
- AmigaOS 3.x: stub ready, toolchain to stabilize
- AmigaOS 4: stub ready, toolchain to install
- AROS: stub ready, toolchain to install

Build on MorphOS:

```text
System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos run
```

Remote build from the Mac:

```sh
ssh user@<morphos-host> 'System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos run'
```

Flow Studio on MorphOS:

- `default.sprj` is the auto-loadable Flow Studio project.
- `telegram-amiga.xprj` can be opened manually from the project requester.
- Open a source file from `Work:Dev/telegram-amiga` or start Flow Studio with
  `PROJECT=Work:Dev/telegram-amiga/default.sprj`.
- The build uses `System:Development/gg/bin/make -f Makefile.morphos all`.
- If opening only the project shows just `Build Rules`, run
  `execute Work:Dev/telegram-amiga/OpenTelegramAmiga.flow`: it opens the project
  together with the main source files, so the Project Lister has an active C
  file from which it can populate Source/Header/Build.
- `telegram-amiga.files` contains the list of the main project files.

Current options:

```text
-h, --help            Show help
-v, --verbose         Enable debug logging
-q, --quiet           Show warnings and errors only
    --data-dir <path> Set application data directory
    --net-test <host> <port>
                      Test DNS resolution and TCP connection
    --http-test <host> <port> <path>
                      Test TCP connect/send/recv with HTTP/1.0
    --https-test <host> <port> <path>
                      Test TLS connect/send/recv with HTTP/1.0
```

Note: through BebboSSH, the remote shell does not always preserve the AmigaDOS
PATH, so the Makefile uses absolute paths to the MorphOS SDK.

Planned Makefiles:

- `Makefile.morphos`
- `Makefile.amigaos3`
- `Makefile.amigaos4`
- `Makefile.aros`

## License

This project is distributed under the MIT License. See `LICENSE` for the full
text.
