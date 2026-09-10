# WebSerialSim

**WebSerialSim** è una libreria leggera per ESP32 che permette di utilizzare un terminale seriale direttamente dal browser tramite **Server-Sent Events (SSE)**.

L'obiettivo del progetto è semplice:

> **avere un monitor seriale web leggero, veloce e affidabile, senza la complessità di un sistema basato su WebSocket.**

La libreria nasce principalmente per applicazioni ESP32 sviluppate con Arduino IDE e può essere utilizzata come terminale di servizio, monitor remoto, console di debug e sistema di visualizzazione dei dati prodotti dal dispositivo.

---

## Caratteristiche principali

- Terminale seriale accessibile dal browser
- Comunicazione browser → ESP32 tramite HTTP
- Comunicazione ESP32 → browser tramite **SSE**
- Un client SSE attivo alla volta
- Controllo del flusso dei dati tramite backpressure
- Trasmissione di blocchi di grandi dimensioni
- Gestione di righe molto lunghe
- Bufferizzazione intelligente dell'output
- History in RAM o PSRAM
- Salvataggio della History su filesystem
- Modalità di scrittura diretta su filesystem
- Configurazione della History durante l'esecuzione
- Timestamp opzionale
- Filtro tramite espressioni regolari lato browser
- Auto-scroll
- Numerazione delle righe
- Visualizzazione e download della History
- Cancellazione della History
- Callback per la gestione dei comandi
- Possibilità di collegare una callback BLE
- Supporto a SD / altri filesystem tramite configurazione
- Nessun utilizzo necessario di `String` per la gestione principale dei dati
- Progettata con particolare attenzione al consumo di RAM

---

# Perché SSE?

WebSerialSim utilizza **Server-Sent Events** per la trasmissione dei dati dall'ESP32 al browser.

La scelta di SSE nasce dall'esigenza di avere un sistema:

- semplice
- leggero
- adatto alla trasmissione continua di testo
- facilmente gestibile dal browser
- con un controllo esplicito del flusso dei dati

Il browser utilizza la normale API JavaScript:

```javascript
const source = new EventSource('/events/serial');
```

I dati vengono inviati attraverso eventi SSE dedicati.

Ad esempio:

```text
serial_print
timestamp
```

La comunicazione dei comandi dal browser all'ESP32 avviene invece tramite una richiesta HTTP POST.

---

# Architettura

La struttura generale è:

```text
                    ┌─────────────────────┐
                    │      Browser        │
                    │                     │
                    │  Web Terminal       │
                    │  Commands            │
                    │  History             │
                    └──────────┬──────────┘
                               │
                    HTTP / SSE │
                               │
                    ┌──────────▼──────────┐
                    │      ESP32          │
                    │                     │
                    │    WebSerialSim      │
                    │                     │
                    │  SSE EventSource     │
                    │  Command Parser      │
                    │  History Manager     │
                    └───────┬──────┬──────┘
                            │      │
                       RAM/PSRAM   │
                            │      │
                            │     SD/FS
                            │
                         Serial
```

L'ESP32 mantiene un singolo client SSE attivo.

Quando un nuovo browser si connette, l'eventuale client precedente viene disconnesso e sostituito.

Questa scelta mantiene l'implementazione semplice e riduce il consumo di risorse.

---

# Backpressure

Uno degli aspetti più importanti di WebSerialSim è la gestione del flusso dati.

Durante la trasmissione verso il browser la libreria controlla lo spazio disponibile nel buffer TCP:

```cpp
clientSSEGlobale->client()->space()
```

e verifica inoltre:

```cpp
clientSSEGlobale->client()->canSend()
```

La funzione:

```cpp
canSendSSE()
```

attende quindi che sia disponibile spazio sufficiente prima di procedere con l'invio.

Questo evita di riempire rapidamente i buffer di rete quando l'ESP32 produce dati più velocemente di quanto il browser riesca a riceverli.

Il meccanismo è particolarmente importante durante la trasmissione di file o grandi quantità di dati.

---

# Trasmissione di grandi quantità di dati

Per i blocchi di grandi dimensioni WebSerialSim utilizza:

```cpp
printBigBuf()
```

La funzione suddivide il contenuto in blocchi di circa **1440 byte**, cercando contemporaneamente di mantenere integre le righe.

Le righe particolarmente lunghe vengono suddivise quando necessario.

Esempio concettuale:

```text
file
 │
 ├── riga
 ├── riga
 ├── riga
 ├── riga molto lunga
 │
 ▼
printBigBuf()
 │
 ├── chunk
 ├── chunk
 ├── chunk
 └── chunk
       │
       ▼
     SSE
       │
       ▼
    Browser
```

Dopo l'invio dei blocchi viene inoltre ceduto tempo al sistema tramite `delay()`/`yield()`, permettendo ad AsyncTCP di gestire correttamente il traffico di rete.

---

# Test di stress

La gestione della trasmissione è stata verificata con un test reale utilizzando un file contenente:

- **27.227 righe**
- **845.181 byte**
- righe superiori a 500 byte
- filtro Regex attivo

Risultato:

```text
27.227 righe
845.181 byte
0 dati persi
```

Tempo totale misurato:

```text
18.667 ms
```

Velocità media:

```text
≈ 45,3 KB/s
≈ 362 kbit/s
```

Il test è stato eseguito utilizzando la versione corrente della libreria e rappresenta un dato sperimentale del progetto, non un limite teorico della tecnologia SSE.

---

# History

WebSerialSim dispone di un sistema History configurabile.

La History può essere mantenuta in:

- RAM interna
- PSRAM
- filesystem
- filesystem in modalità diretta

Il buffer predefinito può essere configurato tramite:

```cpp
#define MAXSIZEBUFFER_HISTORY 4000
```

Se è disponibile PSRAM, la libreria tenta di utilizzare la PSRAM prima di ricorrere alla RAM interna.

---

# Modalità History

Sono disponibili sostanzialmente tre modalità.

## Buffer RAM / PSRAM

I dati vengono accumulati nel buffer circolare.

Quando il buffer raggiunge la dimensione prevista, il contenuto può essere trasferito al filesystem.

```text
Serial
  │
  ▼
RAM / PSRAM
  │
  ▼
History
  │
  ▼
SD
```

Il buffer è circolare e consente di utilizzare una quantità di memoria limitata anche durante sessioni molto lunghe.

---

## Direct FS

Impostando:

```text
CONFIG 0
```

il buffer RAM/PSRAM viene disabilitato e la History può essere scritta direttamente sul filesystem.

```text
Serial
  │
  ▼
History
  │
  ▼
SD
```

Questa modalità riduce il consumo di RAM, a fronte di un maggior numero di operazioni sul filesystem.

---

## Nessuna History

È possibile disabilitare completamente la scrittura su filesystem:

```text
CONFIG NOFS
```

In combinazione con:

```text
CONFIG 0
```

si ottiene una modalità praticamente dedicata al solo monitor live:

```text
Serial
   │
   ▼
 SSE
   │
   ▼
Browser
```

senza buffer History e senza registrazione su filesystem.

---

# Configurazione runtime

Una caratteristica importante di WebSerialSim è la possibilità di modificare la configurazione della History senza ricompilare il programma.

I comandi principali sono:

```text
HISTORY ON
HISTORY OFF
HISTORY FLUSH
HISTORY INFO
```

Per la configurazione:

```text
CONFIG FS
CONFIG NOFS
CONFIG <dimensione>
```

Esempi:

```text
CONFIG 4000
```

imposta un buffer History di 4000 byte.

```text
CONFIG 20000
```

imposta un buffer di 20000 byte.

```text
CONFIG 0
```

attiva la modalità senza buffer.

```text
CONFIG FS
```

abilita la registrazione sul filesystem.

```text
CONFIG NOFS
```

disabilita la registrazione sul filesystem.

---

## Modifica della dimensione del buffer

Per evitare modifiche pericolose durante l'utilizzo del buffer, la dimensione non viene modificata mentre History è attiva.

La sequenza corretta è:

```text
HISTORY OFF
CONFIG 20000
HISTORY ON
```

Se si tenta di modificare la configurazione mentre History è attiva viene richiesto di arrestarla.

Questo permette di evitare riallocazioni o modifiche della struttura del buffer durante l'utilizzo.

---

# History su filesystem

La History può essere salvata su filesystem.

Nella configurazione attuale viene utilizzata la SD:

```cpp
#define HISTORY_SD
```

con:

```cpp
#define FILE_HISTORY "/history.txt"
```

Il file viene aggiornato durante l'utilizzo della History.

È inoltre previsto un limite:

```cpp
#define MAXSIZEFILE_HISTORY 512000
```

Quando il file raggiunge la dimensione massima prevista viene effettuata la rotazione del file.

---

# Funzioni History dal browser

L'interfaccia web permette di gestire la History attraverso appositi comandi.

Sono disponibili:

- **VIEW** — visualizzazione della History
- **DOWNLOAD** — download del file
- **DELETE** — cancellazione della History

La route utilizzata per visualizzare o scaricare la History è:

```text
/buffer?action=view
/buffer?action=down
```

La cancellazione viene effettuata tramite:

```text
/delhistory
```

---

# Timestamp

WebSerialSim può associare un timestamp all'output.

Il formato utilizzato è:

```text
[HH:MM:SS]
```

L'attivazione può essere gestita tramite:

```text
TIMESTAMP ON
TIMESTAMP OFF
```

È inoltre disponibile:

```cpp
setTimestampEnabled(bool enable);
```

I timestamp vengono trasmessi al browser tramite eventi SSE dedicati e utilizzati dall'interfaccia per associare l'informazione temporale alle righe visualizzate.

---

# Comandi

I comandi ricevuti dal browser vengono inviati tramite HTTP POST.

La libreria dispone di un parser interno per i comandi principali e permette inoltre di utilizzare una callback per gestire i comandi specifici dell'applicazione.

La callback ha la forma:

```cpp
using CallbackFunzione = void (*)(char*);
```

Questo permette di utilizzare WebSerialSim anche come una semplice console di comando remota.

---

# Callback BLE

È possibile associare una callback dedicata alla gestione dell'output BLE:

```cpp
using CallbackBLE = void (*)(char*);
```

La callback può essere utilizzata dall'applicazione per inoltrare i dati verso un sistema BLE esterno.

---

# Utilizzo

Un utilizzo tipico consiste nel creare un'istanza:

```cpp
WebSerialSim webSerial;
```

e inizializzarla con il web server:

```cpp
webSerial.begin(&server);
```

A questo punto l'oggetto può essere utilizzato in modo simile a un oggetto `Print`.

Ad esempio:

```cpp
webSerial.println("Hello World!");
```

oppure:

```cpp
webSerial.printfWeb("Temperature: %.2f\n", temperature);
```

L'output viene visualizzato nel terminale del browser secondo la configurazione corrente.

---

# Integrazione con ESPAsyncWebServer

WebSerialSim è progettato per essere utilizzato insieme a:

```text
ESPAsyncWebServer
AsyncTCP
```

Il web server principale viene fornito alla libreria tramite:

```cpp
webSerial.begin(&server);
```

La libreria registra quindi le proprie route HTTP e il proprio `AsyncEventSource`.

Endpoint principale SSE:

```text
/events/serial
```

---

# Interfaccia Web

L'interfaccia integrata contiene un terminale web con diverse funzioni:

- visualizzazione dell'output seriale
- auto-scroll
- numerazione delle righe
- visualizzazione timestamp
- filtro Regex
- invio comandi
- gestione History
- visualizzazione History
- download History
- cancellazione History

L'interfaccia è contenuta nella libreria e viene servita direttamente dall'ESP32.

---

# Gestione della memoria

Il progetto è stato sviluppato con particolare attenzione all'utilizzo della memoria.

La gestione dell'output evita di mantenere grandi quantità di dati sotto forma di `String`.

I dati vengono elaborati principalmente attraverso buffer `char` e lunghezze esplicite.

Per il buffer History viene utilizzata PSRAM quando disponibile e configurata:

```cpp
heap_caps_malloc(
    size,
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
);
```

In assenza di PSRAM viene utilizzata la RAM interna.

---

# Dipendenze

La libreria è pensata per:

- ESP32
- Arduino IDE
- ESPAsyncWebServer
- AsyncTCP

Per la modalità History su SD viene utilizzata la libreria:

```cpp
SD
o LittleFS
o SD_MMC
```

Il supporto al filesystem può essere configurato nel file header.

---

# Filosofia del progetto

WebSerialSim non nasce con l'obiettivo di essere un framework complesso.

La filosofia del progetto è:

```text
Semplice
   +
Leggero
   +
Robusto
   +
Funzionale
```

Particolare attenzione è stata dedicata alla gestione reale dei dati durante la trasmissione, evitando di affidarsi esclusivamente alla velocità teorica della rete.

Il sistema di backpressure, la suddivisione dei blocchi e la gestione del buffer History sono stati sviluppati principalmente attraverso test pratici su ESP32.

---

# Stato del progetto

WebSerialSim è attualmente in fase di sviluppo e sperimentazione.

Le funzionalità principali del terminale SSE e della History sono operative e sono state sottoposte a test con quantità significative di dati.

La libreria è pensata soprattutto per:

- progetti ESP32 personali
- sistemi embedded
- debug remoto
- monitoraggio
- acquisizione dati
- console di servizio
- visualizzazione di file e log

---

# License

Da definire.

---

## Nota

Questa README è una prima bozza e descrive l'architettura e le funzionalità della versione attuale del progetto.

La documentazione definitiva verrà aggiornata insieme alla versione finale della libreria.
