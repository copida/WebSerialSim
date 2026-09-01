# WebSerialSim

Serial terminal WEB con history e streaming via SSE — pensato per ESP32 (Arduino)
WebSerialSim fornisce un terminale seriale remoto tramite web (SSE), supporto alla cronologia (buffer in SRAM/PSRAM o scrittura diretta su file), echo su Serial, integrazione BLE e callback per l'elaborazione dei comandi ricevuti.

## Caratteristiche principali
- Streaming seriale verso browser via Server-Sent Events (SSE) (`/events/serial`).
- Interfaccia web di controllo (pagina: `/serial`) e endpoint per visualizzare il buffer (`/view-buffer`).
- Cronologia log circular buffer in SRAM o PSRAM (configurabile), con possibilità di scrittura diretta su file.
- API semplice per inviare dati verso web/Serial/BLE.
- Supporto echo, callback per gestione comandi, e parsing di comandi remoti (tramite POST `/parsingCmd`).
- Gestione automatica di chunking per evitare MTU e buffer overflow.

## Requisiti
- ESP32 (consigliato) o compatibile con PSRAM opzionale.
- Arduino core per ESP32.
- Librerie:
  - AsyncTCP
  - ESPAsyncWebServer
  - (opzionale) LittleFS / SPIFFS se si vuole attivare salvataggio su filesystem (_TYPE_FS)
- Definizioni di progetto (opzionali):
  - `BUFFER_PSRAM` — abilita l'allocazione PSRAM (se presente).
  - `_TYPE_FS` e `FS_STORY` — abilita scrittura diretta su file system.
  - `OUTBLE` — abilita il callback BLE.

## Installazione
1. Copia i file della libreria (cartella `src/` e relativi header) nella cartella `libraries` del tuo progetto o installala come libreria.
2. Assicurati di includere e inizializzare `AsyncWebServer` nello sketch.
3. Includi l'header:
   ```cpp
   #include "WebSerialSim.h"
   ```

## Uso rapido (esempio)
Esempio minimale di integrazione in uno sketch ESP32 con AsyncWebServer:

```cpp
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "WebSerialSim.h"

AsyncWebServer server(80);
WebSerialSim webSerial;

void onCommand(char* cmd, char* param) {
  // Esempio: echo del comando ricevuto
  webSerial.printfWeb("Comando ricevuto: %s\n", cmd);
}

void setup() {
  Serial.begin(115200);
  // inizializza il server nel loop principale (o come preferisci)
  server.begin();

  // Avvia WebSerialSim collegandolo al server esistente
  webSerial.begin(&server);

  // Registra callback per comandi in ingresso
  webSerial.setCallback(onCommand);

  // Abilita history (alloca buffer)
  webSerial.modestory(true);

  // Abilita echo verso Serial quando i dati arrivano da web
  webSerial.echoOnOff(true);
}

void loop() {
  // Deve essere chiamato regolarmente per gestione non bloccante
  webSerial.taskList();

  // altri task dell'applicazione...
}
```

## API e metodi principali
- WebSerialSim() — costruttore.
- void begin(AsyncWebServer* mainServer) — registra rotte e avvia la parte web (SSE).
- size_t write(uint8_t) / write(const uint8_t*, size_t) — scrive dati (vengono inviati a Serial / web / BLE in base allo stato).
- void printfWeb(const char* format, ...) — stampa formattato verso web/Serial.
- void printWeb(char*) — invia una stringa (gestisce chunk grandi).
- void setCallback(CallbackFunzione cb) — imposta callback per comandi ricevuti (firma: void (*)(char*, char*)).
- void setCallBLE(CallbackBLE cb) — imposta callback BLE (se abilitato).
- void echoOnOff(bool onoff) — attiva/disattiva echo su Serial.
- bool inputEXT(char* inExt, int lenb) — svincola input esterno (es. Bluetooth).
- void taskList() — funzione non bloccante da chiamare regolarmente (gestisce buffer, parsing).
- void modestory(bool action) — attiva/disattiva registrazione history.
- void setbuffer(size_t _dimbuffer) — imposta dimensione buffer history.
- void fViewHistory(), fHistoryClear(), fHistoryFlush(), infoSerBuf() — gestione history.
- bool canSendSSE(size_t requiredSpace) — verifica spazio per invio SSE.

Nota: cerca nel file `src/WebSerialSim.cpp` per l'elenco completo delle funzioni e dettagli implementativi.

## Endpoints HTTP e SSE
- GET /serial — pagina HTML del terminale (in bundle nella libreria).
- GET /view-buffer — restituisce il contenuto della history (o file se `directFS` attivo).
- GET /get-clientcount — restituisce il numero di client SSE connessi.
- POST /parsingCmd — usato dal client web per inviare comandi: i dati POST devono contenere un token client (puntatore) seguito da ":" e la stringa del comando (es. "1070340744:LED ON").

SSE:
- Endpoint: `/events/serial` — l'EventSource invia eventi con tipo `serial_print` e `client_count` per notifiche.

## Buffer history
- Circular buffer configurabile (`dimSerBuf`), con comportamento di wrap/overwriting automatico.
- Possibilità di allocare in PSRAM (se definito e presente) o in SRAM.
- Opzione `directFS` per scrittura diretta su file (se filesystem abilitato).
- Comandi utili: `HISTORY ON`, `HISTORY OFF`, `HISTORY VIEW`, `HISTORY CLEAR`, `HISTORY FLUSH`, `HISTORY INFO`.

## Suggerimenti / Troubleshooting
- Assicurati che il buffer history sia almeno ~1500 byte (il codice emette un warning per buffer troppo piccoli).
- Se usi PSRAM definisci `BUFFER_PSRAM` e verifica `psramFound()` (ESP32).
- Per grandi volumi di output la libreria chunka automaticamente per restare sotto l'MTU.
- Verifica che `AsyncWebServer` e `AsyncEventSource` siano correttamente inizializzati prima di chiamare `webSerial.begin(&server)`.
- Se gli SSE non arrivano controlla lo stato del client (`activeClients`) e la funzione `canSendSSE()`.

## Licenza
Questo repository non dichiara esplicitamente una licenza nel codice mostrato. Ti suggerisco di aggiungere una licenza (ad esempio MIT) se vuoi condividere o riutilizzare liberamente la libreria.

## Contribuire
Se vuoi, posso:
- aggiungere questo README direttamente al repository;
- migliorare esempi e la documentazione API (aggiungere snippet più dettagliati per tutte le funzioni);
- creare uno sketch di esempio completo in `examples/`.

Per contributi: apri una issue o PR nel repository con la tua proposta.
