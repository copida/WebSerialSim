# WebSerialSim

**Serial terminal web remoto con cronologia persistente e streaming non-bloccante via SSE** — Ottimizzato per ESP32 (Arduino).

WebSerialSim fornisce un **terminale seriale remoto** tramite browser web (SSE), supporto alla **cronologia circolare** (buffer in SRAM/PSRAM/SD), **echo su Serial**, integrazione BLE e callback per la gestione remota di comandi. Perfetto per debug e monitoraggio remoto di dispositivi embedded.

---

## 🚀 Perché WebSerialSim?

### Confronto con altre librerie di monitoraggio seriale

| **Feature** | **WebSerialSim** | **SerialMonitor.js** | **altre lib WebSocket** |
|-------------|:----------------:|:-------------------:|:---------------------:|
| **Supporto output 1MB+** | ✅ | ❌ (crash ~100KB) | ❌ (crash ~100KB) |
| **Non-bloccante** | ✅ | ⚠️ (bloccante) | ⚠️ (bloccante) |
| **Buffer circolare** | ✅ (SRAM/PSRAM/SD) | ❌ (solo RAM) | ❌ (solo RAM) |
| **Storage persistente** | ✅ (SD/LittleFS) | ❌ | ❌ |
| **API nativa (Print)** | ✅ | ❌ (custom) | ❌ (custom) |
| **Tecnologia** | SSE (leggera) | WebSocket | WebSocket |
| **Timeout intelligente** | ✅ (50ms) | ❌ | ❌ |
| **Fallback Serial** | ✅ | ❌ | ❌ |
| **Callback e parsing comandi** | ✅ | ⚠️ (limitato) | ⚠️ (limitato) |
| **Supporto BLE** | ✅ | ❌ | ❌ |

---

## ⭐ Caratteristiche principali

### **1. Streaming SSE non-bloccante**
- Trasmissione dati verso browser via **Server-Sent Events** (`/events/serial`)
- **Non-bloccante**: il loop principale non aspetta mai il client
- **Timeout intelligente** (50ms): accumula caratteri fino a newline o timeout
- **Fallback su Serial** se il client è lento o disconnesso

### **2. Buffer circolare scalabile**
```
┌─────────────────────────────────────┐
│  SRAM (limitato) / PSRAM (8MB) / SD │  ← Scegli il tuo storage
│  Buffer circolare che wrappa        │
│  Automaticamente salva su SD al wrap│
└─────────────────────────────────────┘
```
- Circular buffer configurabile (default 4KB, scalabile fino a GB con SD)
- Allocazione in **SRAM** (64KB), **PSRAM** (4-8MB), o **SD/LittleFS** (illimitato)
- **Wrap automatico**: quando il buffer è pieno, i dati vecchi vengono sovrascritti
- **Flush intelligente su SD** prima di sovrascrivere dati importanti

### **3. Interfaccia web integrata**
```
GET /serial          → Terminale web interattivo
GET /view-buffer     → Visualizza la cronologia completa
GET /get-clientcount → Numero di client connessi
POST /parsingCmd     → Invia comandi remoti
```

### **4. API semplice (eredita da Print)**
```cpp
webSerial.print("Messaggio");
webSerial.printf("Valore: %d\n", 42);
webSerial.println("Test");
// Funziona come Serial! Niente API custom.
```

### **5. Gestione intelligente dei dati grandi**
- **Chunking automatico** per evitare MTU e buffer overflow
- Chunking non-bloccante con `delay(2)` fra i chunk
- Supporta output di **1MB+ senza crash**

### **6. Comandi HISTORY integrati**
```
HISTORY ON      → Attiva registrazione
HISTORY OFF     → Disattiva registrazione
HISTORY VIEW    → Visualizza buffer
HISTORY CLEAR   → Svuota buffer
HISTORY FLUSH   → Salva su SD
HISTORY INFO    → Statistiche spazio
```

### **7. Multi-destinazione output**
- **Web**: via SSE con fallback automático
- **Serial**: locale sul dispositivo
- **BLE**: callback configurabile
- **Storage**: buffer RAM + SD persistente

### **8. Callback e parsing comandi**
```cpp
void onCommand(char* cmd, char* param) {
    webSerial.printfWeb("Comando: %s\n", cmd);
    // Elabora il comando remoto
}
webSerial.setCallback(onCommand);
```

---

## 📊 Caso d'uso: perché SSE e non WebSocket?

**Per un monitor seriale, SSE è la scelta corretta:**

| Aspetto | SSE | WebSocket |
|--------|-----|-----------|
| **Direzione** | Server→Client (monodirezionale) | Client↔Server (bidirezionale) |
| **Overhead** | Minimo (text-based) | Medio (binary framing) |
| **Connessione** | HTTP/1.1 standard | Upgrade HTTP → WS |
| **Robustezza** | Alta (fallback HTTP) | Media (richiede WS support) |
| **Caso d'uso** | Streaming dati | Chat, gaming, real-time bidirectional |
| **Perf per 1MB** | ✅ OK | ❌ Crash |

**La tua app**: mandare dati dal device → browser (monodirezionale) → **SSE perfetto** ✅

---

## 🛠️ Requisiti

### Hardware
- **ESP32** (consigliato) o compatibile
- ✅ **PSRAM opzionale** (4-8MB su ESP32-S3, S2, WROVER)
- ✅ **SD card opzionale** (per storage illimitato)

### Software
```cpp
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "WebSerialSim.h"
```

### Librerie richieste
- **AsyncTCP** (PlatformIO: `asynctcp`)
- **ESPAsyncWebServer** (PlatformIO: `espassyncwebserver`)

### Librerie opzionali
- **LittleFS/SPIFFS** — per storage su filesystem
- **SD** — per storage su SD card

### Definizioni di progetto (platformio.ini)
```ini
[env:esp32]
build_flags =
    -D BUFFER_PSRAM          # Abilita PSRAM
    -D HISTORY_SD            # Scrive su SD (non RAM)
    -D OUTBLE                # Abilita callback BLE
```

---

## 📦 Installazione

### Opzione 1: Copia manuale
```bash
git clone https://github.com/copida/WebSerialSim.git
cp -r WebSerialSim/src/* <tuoProgetto>/lib/WebSerialSim/
```

### Opzione 2: PlatformIO (coming soon)
```ini
lib_deps =
    copida/WebSerialSim
```

### Opzione 3: Arduino IDE
1. Scarica il `.zip` da GitHub
2. Sketch → Includi libreria → Aggiungi libreria .ZIP
3. Seleziona il file scaricato

---

## 🚀 Uso rapido

### Sketch minimalista
```cpp
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "WebSerialSim.h"

AsyncWebServer server(80);
WebSerialSim webSerial;

void onCommand(char* cmd, char* param) {
    webSerial.printfWeb("Comando ricevuto: %s\n", cmd);
}

void setup() {
    Serial.begin(115200);
    
    // Connetti a WiFi (non mostrato)
    WiFi.mode(WIFI_STA);
    WiFi.begin("SSID", "PASSWORD");
    while (WiFi.status() != WL_CONNECTED) delay(100);
    
    // Avvia server web
    server.begin();
    
    // Inizializza WebSerialSim
    webSerial.begin(&server);
    webSerial.setCallback(onCommand);
    webSerial.modestory(true);    // Attiva history
    webSerial.setbuffer(4096);    // Buffer 4KB
    webSerial.echoOnOff(true);    // Echo su Serial
    
    webSerial.println("WebSerialSim avviato!");
    webSerial.printf("WiFi: %s\n", WiFi.localIP().toString().c_str());
}

void loop() {
    webSerial.taskList();  // Non bloccante
    
    // Tuoi task...
    delay(10);
}
```

### Accedi al terminale
```
http://<IP_ESP32>/serial
```

---

## 📡 API Reference

### Inizializzazione
```cpp
void begin(AsyncWebServer* mainServer);
```
Registra le rotte web e avvia SSE.

### Output (eredita da Print)
```cpp
void print(const char*);
void println(const char*);
void printf(const char* format, ...);
void printfWeb(const char* format, ...);  // Diretto a web
```

### Buffer e History
```cpp
void modestory(bool action);           // Attiva/disattiva history
void setbuffer(size_t _dimbuffer);     // Imposta dimensione buffer (byte)
void fViewHistory();                   // Visualizza contenuto buffer
void fHistoryClear();                  // Svuota buffer
void fHistoryFlush();                  // Salva buffer su SD
void infoSerBuf();                     // Mostra statistiche
```

### Comandi
```cpp
bool inputEXT(char* inExt, int lenb);              // Input esterno (Bluetooth)
void setCallback(CallbackFunzione cb);             // Callback per comandi
void echoOnOff(bool onoff);                        // Echo su Serial
void taskList();                                   // Main task (non bloccante)
```

### SSE
```cpp
bool checkClientSSE();                 // Ci sono client connessi?
bool canSendSSE(size_t requiredSpace); // Spazio disponibile?
```

---

## 🌐 Endpoints HTTP

| Endpoint | Metodo | Descrizione |
|----------|--------|-------------|
| `/serial` | GET | Pagina HTML del terminale |
| `/view-buffer` | GET | Scarica la cronologia completa |
| `/get-clientcount` | GET | Numero di client SSE connessi |
| `/parsingCmd` | POST | Invia comandi remoti |
| `/events/serial` | SSE | Stream dati (evento: `serial_print`, `client_count`) |

### Esempio POST
```bash
curl -X POST http://192.168.1.100/parsingCmd \
  -H "Content-Type: text/plain" \
  -d "1070340744:HISTORY INFO"
  # Format: <clientID>:<comando>
```

---

## 🔧 Configurazione avanzata

### Personalizzare dimensioni buffer
```cpp
#define MAXSIZEBUFFER_HISTORY 4000  // Piccolo (SRAM)
// oppure
#define MAXSIZEBUFFER_HISTORY 65536 // Grande (PSRAM)
// oppure
#define MAXSIZEBUFFER_HISTORY 0     // Scrittura diretta su SD (senza RAM buffer)
```

### Scegliere storage
```ini
# platformio.ini
build_flags =
    -D HISTORY_SD        # Scrive su SD (/history.txt)
    # -D HISTORY_SDMMC   # Scrive su SD_MMC
    # -D HISTORY_LittleFS # Scrive su LittleFS
```

### Abilitare PSRAM
```ini
build_flags = -D BUFFER_PSRAM
```
Richiede `psramFound()` e `ps_malloc()` (built-in su Arduino ESP32).

### Callback BLE
```cpp
void bleOutput(char* data) {
    // Invia data al modulo BLE
}

#define OUTBLE
webSerial.setCallBLE(bleOutput);
```

---

## 🐛 Troubleshooting

### "Buffer too small" warning
```
⚠️ Attenzione buffer too small ..almeno 1500
```
**Soluzione**: aumenta `MAXSIZEBUFFER_HISTORY` a >= 1500 byte.

### SSE non arrivano al browser
**Controlla**:
1. ESP32 e browser sulla stessa rete WiFi
2. Firewall blocca porta 80
3. `server.begin()` è stato chiamato prima di `webSerial.begin(&server)`
4. Browser supporta SSE (edge, firefox, chrome OK; IE 11 NO)

### Crash durante output grandi
**Cause comuni**:
1. Buffer troppo piccolo → aumenta `MAXSIZEBUFFER_HISTORY`
2. PSRAM non rilevata → disabilita `BUFFER_PSRAM` se non disponibile
3. Stack overflow → riduci altre allocazioni

**Soluzione**:
```cpp
webSerial.setbuffer(8192);  // Aumenta buffer
webSerial.modestory(true);   // Attiva history
// o usa SD:
#define MAXSIZEBUFFER_HISTORY 0  // Scrittura diretta su SD
```

### Memory leak
**Assicurati di chiamare** `taskList()` regolarmente nel loop:
```cpp
void loop() {
    webSerial.taskList();  // ← OBBLIGATORIO
    // altri task
}
```

---

## 📋 Roadmap futuri

- [ ] **Timestamp automatico** per ogni linea di log
- [ ] **Regex/filter real-time** nel frontend (search)
- [ ] **Color coding** (ANSI escape codes per ERROR/WARN/DEBUG)
- [ ] **Export CSV** della history
- [ ] **Statistiche in tempo reale** (bytes/sec, uptime)
- [ ] **Dark/Light mode** nell'interfaccia web
- [ ] **Mobile-responsive UI**

---

## 📄 Licenza

MIT License — Vedi [LICENSE](LICENSE) per dettagli.

---

## 🤝 Contribuire

Feedback, bug report e PR sono benvenuti!

1. Apri una **Issue** per bug o feature request
2. Fai un fork e crea un branch: `git checkout -b feature/nome-feature`
3. Commit: `git commit -am 'Add feature: ...'`
4. Push: `git push origin feature/nome-feature`
5. Apri una **Pull Request**

---

## 📞 Support

- 📖 Vedi la sezione [API Reference](#-api-reference) sopra
- 🐛 Apri un'issue su GitHub
- 💬 Discussioni: GitHub Discussions (coming soon)

---

## 🏆 Credits

Sviluppato per ESP32 debugging e monitoraggio remoto di dispositivi embedded.

**Made with ❤️ for makers & embedded engineers**
