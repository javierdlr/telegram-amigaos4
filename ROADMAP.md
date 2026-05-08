# Roadmap

Author: Michele Dipace <michele.dipace@kaffeine.net>

Telegram Amiga e' un progetto comunitario senza finalita' commerciali. La
roadmap e' volutamente pragmatica: ogni fase deve produrre qualcosa di
compilabile e verificabile su almeno una piattaforma Amiga-like reale.

## Fase 1: base portabile

- Struttura del progetto cross-platform
- Build separati per MorphOS, AmigaOS 3.x, AmigaOS 4 e AROS
- Logging e configurazione comuni
- API TCP portabile
- Test di rete da riga di comando

## Fase 2: HTTP e TLS

- HTTP minimale sopra TCP
- Backend TLS/HTTPS opzionale
- Validazione certificati
- Stabilizzazione di OpenSSL/AmiSSL su MorphOS
- Scelta della libreria TLS piu' adatta per AmigaOS 3.x

## Fase 3: API Telegram

- Chiamate HTTPS verso Bot API o API Telegram adatte al target
- Parsing JSON minimale
- Gestione configurazione account/token
- Primi test di ricezione messaggi

## Fase 4: interfaccia utente

- Interfaccia testuale iniziale per debug
- Astrazione UI comune
- Backend UI specifici per piattaforma
- Esperienza nativa per MorphOS e AmigaOS dove possibile

## Fase 5: client usabile

- Lista chat o conversazioni supportate
- Lettura e invio messaggi
- Persistenza locale minima
- Packaging per le piattaforme supportate

## Fuori scope iniziale

- Cifratura end-to-end delle secret chat
- Supporto completo a media pesanti
- Compatibilita' totale con tutte le funzioni dei client Telegram moderni
