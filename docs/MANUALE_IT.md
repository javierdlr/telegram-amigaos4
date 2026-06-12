# Telegram Amiga — Manuale rapido (italiano)

*English version: [MANUAL_EN.md](MANUAL_EN.md)*

Telegram vero, in finestra console, sul tuo Amiga. Niente browser, niente
proxy esterni: il client parla MTProto direttamente con i server Telegram.
Gira su AmigaOS 3.x (68k), AmigaOS 4.x, MorphOS, AROS i386 e AROS x86_64.
È software sperimentale (pre-alpha): usalo con la testa.

## 1. Scarica

Prendi lo zip della tua piattaforma dall'ultima release:
https://github.com/kaffeine1/telegram-amiga/releases

- `Telegram-amigaos3-<data>.zip` — AmigaOS 3.x, 68020+, niente ixemul
- `Telegram-amigaos4-<data>.zip` — AmigaOS 4.x PPC
- `Telegram-morphos-<data>.zip` — MorphOS PPC
- `Telegram-aros-i386-<data>.zip` — AROS x86 ABIv0
- `Telegram-aros-x86_64-<data>.zip` — AROS x86_64 (hosted o sistemi con
  kickstart/SDK di linea trunk; NON gira su AROS One v0.38)

## 2. Installa

Scompatta dove vuoi (RAM: va benissimo per provare, una cartella su disco
se vuoi conservare il login). Dentro trovi:

- `TelegramAmiga` + icona — il programma da avviare (doppio click)
- `telegram-test` — il binario vero e proprio
- `telegram-api.txt` — credenziali API pubbliche del progetto (già pronte)
- `README.txt` — istruzioni in inglese

## 3. Primo avvio e login

Doppio click su **TelegramAmiga**. La prima volta parte la procedura di
login: inserisci il numero di telefono in formato internazionale
(+39...), poi il codice che Telegram ti manda sull'app ufficiale, ed
eventualmente la password 2FA se la usi. Il login resta salvato in
`telegram-auth.bin`: dal secondo avvio entri diretto.

Consiglio: se hai dubbi, prova prima con un account secondario.

## 4. La schermata

Layout a tutta finestra stile AmIRC/XChat:

- in alto la **barra di stato** col nome della chat aperta
- al centro la **conversazione** che scorre (orari `[HH:MM]`, separatori
  di giornata, emoji rese come emoticon classiche)
- in basso la **riga di scrittura**, sempre al suo posto

Scrivi e premi Invio per mandare ([ok] = consegnato al server). Invio a
vuoto = leggi subito i nuovi messaggi (comunque arrivano da soli).

## 5. Cambiare chat al volo

- **F1..F10** → chat 1..10 (con Shift: 11..20)
- **Tab** → torna alla chat precedente (avanti e indietro)
- **un numero + Invio** → chat con quell'indice
- `/peers` → elenco chat con indici e non-letti
- `/add nome` (o @username o link t.me) → cerca su Telegram e aggiungi
- `/search testo` → cerca tra le chat già in elenco

Quando arriva un messaggio da un'altra chat compare una riga tipo
`[3] Mario Rossi: ciao...` e lo schermo lampeggia: premi F3 (o scrivi 3)
e sei lì.

## 6. Comandi utili

- `/history` — ultimi messaggi della chat aperta
- `/watch sec` — ogni quanti secondi controllare i nuovi (`/watch off`)
- `/bell` — lampeggio notifiche on/off
- `/color` — colori on/off
- `/resize` — ridisegna il layout dopo che hai ridimensionato la finestra
  (serve sui terminali che non avvisano da soli, es. la shell MorphOS)
- `/remove n` — togli una chat dall'elenco
- `/help` — tutto l'elenco
- `/quit` — esci (funziona anche **Ctrl+C**, ovunque)

La finestra si può ridimensionare quando vuoi: il layout si riadatta.

## 7. Note per piattaforma

- **AmigaOS 3.x**: colori del testo senza sfondo (alcune shell hanno la
  pen di sfondo rossa); chi vuole il look scuro: `--ui-theme dark`.
  Testato su 68030+; serve uno stack TCP (Roadshow, AmiTCP...).
- **AmigaOS 4.x**: tutto attivo di default.
- **MorphOS**: i colori sono spenti di default (limite console del
  sistema). Le notifiche tra chat funzionano "a rate": arrivano entro
  ~12 secondi invece che all'istante, per proteggere la rete lenta
  della piattaforma (`/diff off` le spegne). Lettura auto ogni 12s —
  regolabile con `/watch`. Avvia da disco (es. `Work:`), non da RAM:.
- **AROS**: se avvii da icona e la console non supporta il full-screen,
  il client lo capisce da solo e passa al flusso classico riga-per-riga.

## 8. Aggiornare da una versione precedente

Scompatta la nuova versione e copia dentro la cartella i tuoi
`telegram-auth.bin` (login) e `telegram-peers.txt` (elenco chat) dalla
vecchia. Fine: niente nuovo login.

## 9. Sicurezza, per favore

- **Non pubblicare mai** `telegram-auth.bin`: è la tua sessione.
- Niente screenshot con numeri di telefono, codici di login o messaggi
  privati.
- Le credenziali in `telegram-api.txt` sono quelle pubbliche del
  progetto: chi vuole può usare le proprie (vedi README).

## 10. Problemi?

Segnalazioni nel gruppo con uno screenshot e il modello di macchina/OS:
è così che sono stati trovati e chiusi quasi tutti i bug. Buone chat
dall'Amiga! 🖥️
