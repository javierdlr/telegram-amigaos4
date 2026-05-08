# Telegram Amiga

Author: Michele Dipace <michele.dipace@kaffeine.net>

Bootstrap minimale per un futuro client Telegram cross-platform:

- `core/`: logica portabile
- `include/`: interfacce pubbliche interne
- `platforms/*/`: adattatori specifici per sistema
- `src/main.c`: entry point sottile

Target iniziali:

- MorphOS: attivo e verificato
- AmigaOS 3.x: stub pronto, toolchain da stabilizzare
- AmigaOS 4: stub pronto, toolchain da installare
- AROS: stub pronto, toolchain da installare

Build su MorphOS:

```text
System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos run
```

Build remoto dal Mac:

```sh
ssh kaffeine@192.168.0.9 'System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos run'
```

Nota: via BebboSSH la shell remota non mantiene sempre il PATH AmigaDOS, quindi il Makefile usa percorsi assoluti verso il MorphOS SDK.

Makefile previsti:

- `Makefile.morphos`
- `Makefile.amigaos3`
- `Makefile.amigaos4`
- `Makefile.aros`
