# Contributing

Author: Michele Dipace <michele.dipace@kaffeine.net>

Thank you for your interest in Telegram Amiga. This project is a gift to the
Amiga community and welcomes practical, verifiable contributions that fit the
cross-platform spirit of the repository.

## Principles

- Keep portable code in the `core/` layer whenever possible.
- Isolate system differences under `platforms/<target>/`.
- Prefer small, buildable steps over large rewrites.
- Avoid dependencies that are difficult to obtain on Amiga-like systems.
- Document the tests you ran, including platform, toolchain and command.

## AI Agent Usage

The project accepts contributions prepared with AI tools or AI agents, provided
they are reviewed by a person and accompanied by real or reproducible tests. AI
agents are treated as support tools: the value of a contribution remains in the
quality of the code, the clarity of the patch and the verification on
Amiga-like targets.

When possible, state whether a change was tested through SSH on real hardware,
on an emulator, or only through local compilation.

## Targets

The planned targets are:

- AmigaOS 3.x
- MorphOS
- AmigaOS 4
- AROS

You do not need to own every system to contribute, but a change should not
intentionally break the other targets.

## MorphOS Build

```text
System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos all
```

TLS/OpenSSL on MorphOS is optional:

```text
System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos ENABLE_TLS=1 all
```

Use `ENABLE_TLS=1` only if the OpenSSL/AmiSSL environment is ready and stable.

## Before Proposing a Patch

- Build at least the target you changed.
- Run the closest available test, for example `--net-test`, `--http-test` or
  `--https-test`.
- Keep the existing author/license headers.
- Update `README.md` or `ROADMAP.md` if you change behavior or priorities.

## License

By contributing to the project, you agree that your contribution is distributed
under the repository's MIT License.
