# Telegram Amiga

Author: Michele Dipace <michele.dipace@kaffeine.net>

Bootstrap minimale per un futuro client Telegram cross-platform:

- `core/`: logica portabile
- `include/`: interfacce pubbliche interne
- `platforms/*/`: adattatori specifici per sistema
- `src/main.c`: entry point sottile

Moduli core iniziali:

- `tg_config`: parsing minimale degli argomenti
- `tg_log`: logging portabile delegato alla piattaforma
- `tg_http`: HTTP/1.0 minimale sopra `tg_net`
- `tg_net`: API TCP portabile con implementazione MorphOS iniziale

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

Flow Studio su MorphOS:

- `default.sprj` e' il progetto Flow Studio auto-caricabile.
- `telegram-amiga.xprj` e' apribile manualmente dal requester progetto.
- Apri un file sorgente da `Work:Dev/telegram-amiga` oppure avvia Flow Studio con `PROJECT=Work:Dev/telegram-amiga/default.sprj`.
- Il build usa `System:Development/gg/bin/make -f Makefile.morphos all`.
- Se aprendo solo il progetto vedi solo `Build Rules`, esegui `execute Work:Dev/telegram-amiga/OpenTelegramAmiga.flow`: apre il progetto insieme ai sorgenti principali, cosi' il Project Lister ha un file C attivo da cui popolare Source/Header/Build.
- `telegram-amiga.files` contiene la lista dei file principali del progetto.

Opzioni attuali:

```text
-h, --help            Mostra l'help
-v, --verbose         Abilita log debug
-q, --quiet           Mostra solo warning ed errori
    --data-dir <path> Imposta la directory dati
    --net-test <host> <port>
                      Testa risoluzione DNS e connessione TCP
    --http-test <host> <port> <path>
                      Testa connect/send/recv TCP con HTTP/1.0
```

Nota: via BebboSSH la shell remota non mantiene sempre il PATH AmigaDOS, quindi il Makefile usa percorsi assoluti verso il MorphOS SDK.

Makefile previsti:

- `Makefile.morphos`
- `Makefile.amigaos3`
- `Makefile.amigaos4`
- `Makefile.aros`
