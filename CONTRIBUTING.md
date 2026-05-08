# Contributing

Author: Michele Dipace <michele.dipace@kaffeine.net>

Grazie per l'interesse in Telegram Amiga. Il progetto nasce come regalo alla
community Amiga e accetta contributi pratici, verificabili e compatibili con lo
spirito cross-platform del repository.

## Principi

- Mantieni il codice portabile nel layer `core/` quando possibile.
- Isola le differenze di sistema sotto `platforms/<target>/`.
- Preferisci piccoli passi compilabili a grandi riscritture.
- Evita dipendenze difficili da reperire sui sistemi Amiga-like.
- Documenta i test fatti, includendo piattaforma, toolchain e comando usato.

## Uso di agenti AI

Il progetto accetta contributi preparati anche con strumenti o agenti AI, purche'
siano revisionati da una persona e accompagnati da test reali o riproducibili.
Gli agenti sono considerati strumenti di supporto: il valore del contributo resta
nella qualita' del codice, nella chiarezza della patch e nella verifica sui
target Amiga-like.

Quando possibile, indica se una modifica e' stata provata tramite SSH su una
macchina reale, su emulatore o solo con compilazione locale.

## Target

I target previsti sono:

- AmigaOS 3.x
- MorphOS
- AmigaOS 4
- AROS

Non e' necessario avere tutti i sistemi per contribuire, ma una modifica non
dovrebbe rompere intenzionalmente gli altri target.

## Build MorphOS

```text
System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos all
```

TLS/OpenSSL su MorphOS e' opzionale:

```text
System:Development/gg/bin/make -C Work:Dev/telegram-amiga -f Makefile.morphos ENABLE_TLS=1 all
```

Usa `ENABLE_TLS=1` solo se l'ambiente OpenSSL/AmiSSL e' pronto e stabile.

## Prima di proporre una patch

- Compila almeno il target che hai modificato.
- Esegui il test piu' vicino alla modifica, per esempio `--net-test`,
  `--http-test` o `--https-test`.
- Mantieni le intestazioni autore/licenza esistenti.
- Aggiorna `README.md` o `ROADMAP.md` se cambi comportamento o priorita'.

## Licenza

Contribuendo al progetto accetti che il contributo sia distribuito sotto la
licenza MIT del repository.
