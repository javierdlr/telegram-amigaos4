# Telegram Amiga

Author: Michele Dipace <michele.dipace@kaffeine.net>

License: MIT

Progetto comunitario senza finalita' commerciali, nato come regalo alla community
Amiga per esplorare la possibilita' di un client Telegram moderno su sistemi
Amiga-like.

Stato attuale: bootstrap tecnico iniziale, non ancora un client Telegram
utilizzabile.

## Obiettivo

Costruire, un passo alla volta, un client Telegram cross-platform per:

- AmigaOS 3.x
- MorphOS
- AmigaOS 4
- AROS

Il progetto privilegia codice portabile in C, backend separati per piattaforma e
test incrementali su hardware o sistemi reali.

## Struttura

Bootstrap minimale:

- `core/`: logica portabile
- `include/`: interfacce pubbliche interne
- `platforms/*/`: adattatori specifici per sistema
- `src/main.c`: entry point sottile

Moduli core iniziali:

- `tg_config`: parsing minimale degli argomenti
- `tg_log`: logging portabile delegato alla piattaforma
- `tg_http`: HTTP/1.0 minimale sopra `tg_net`
- `tg_net`: API TCP portabile con implementazione MorphOS iniziale
- `tg_tls`/`tg_https`: TLS/HTTPS minimale con backend MorphOS OpenSSL iniziale

Nota TLS: il backend MorphOS iniziale usa OpenSSL/AmiSSL con SNI, ma la validazione dei certificati non e' ancora abilitata. Questo e' sufficiente per test di connettivita', non ancora per uso sicuro.

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
    --https-test <host> <port> <path>
                      Testa connect/send/recv TLS con HTTP/1.0
```

Nota: via BebboSSH la shell remota non mantiene sempre il PATH AmigaDOS, quindi il Makefile usa percorsi assoluti verso il MorphOS SDK.

Makefile previsti:

- `Makefile.morphos`
- `Makefile.amigaos3`
- `Makefile.amigaos4`
- `Makefile.aros`

## Licenza

Questo progetto e' distribuito sotto licenza MIT. Vedi `LICENSE` per il testo completo.
