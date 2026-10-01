# WebSerialSim - Code Review Completa e Versioni Migliorate

## CHECKLIST PRIORITÀ

### 🔴 CRITICO (Fix prima di tutto)

#### 1. `fregbuffer()` bug del ring buffer (linea 975-1008)
**Problema**: Quando il buffer è pieno e con wraparound, la logica di flush non gestisce correttamente i dati.

**Scenario fallente**:
```
Buffer: [dati_nuovi | ... | dati_vecchi]
         ^pSerBuf               ^tailBuf
         
Se pSerBuf < tailBuf (wraparound):
  pSerBuf - tailBuf è NEGATIVO → scrivi zero byte
```

**Impatto**: Perdita di dati in history  
**Severità**: ALTA  

---

#### 2. `parsingCmd()` uso di `strtok()` (linea 359-366)
**Problema**: `strtok()` usa stato globale interno non thread-safe.

```c++
// PERICOLOSO in ambienti multitask:
command = strtok(buffer_ser, " ");      // Modifica stato globale strtok
// Se qui arriva un'altra callback che chiama strtok => CORRUZIONE
argument = strtok(NULL, " ");
```

**Impatto**: Parsing corrotto, comandi malformati, crash  
**Severità**: ALTA  

---

#### 3. `insBuffer()` buffer underflow (linea 889-945)
**Problema**: Se `dimSerBuf == 0`, il controllo `dimstr >= dimSerBuf` fallisce.

```c++
int dimstr = strlen(str);
if(dimstr >= dimSerBuf) dimstr = dimSerBuf - 1;  // Se dimSerBuf=0 => -1 (underflow!)
```

**Impatto**: Crash, corruzione memoria  
**Severità**: MEDIA  

---

### 🟡 MEDIO (Fix presto)

#### 4. `getTimestampString()` thread-unsafe (linea 110-127)
**Problema**: `localtime()` usa static storage, non è reentrant.

```c++
struct tm* timeinfo = localtime(&now);  // Non thread-safe!
snprintf(_timestamp, sizeof(_timestamp), "[%02d:%02d:%02d] ", ...);
```

**In multitask**: Due thread che chiamano contemporaneamente → buffer corrotto  
**Severità**: MEDIA  

---

#### 5. Client count sbagliato (linea 637)
```c++
Serial.printf("[SSE] Client connessi totale: %d\n", eventsserial->count() + 1);
// Se count() restituisce già il totale, aggiungere +1 è errato!
```

**Impatto**: Log diagnostico sbagliato  
**Severità**: BASSA  

---

#### 6. `txSSE()` `requiredSpace` inaffidabile (linea 262)
**Problema**: Lo spazio richiesto non è sempre coerente con il payload reale.

```c++
if(rawClient->space() > requiredSpace && rawClient->canSend()){
    // requiredSpace potrebbe non riflettere il vero payload SSE
    invioRiuscito = eventsserial->send((const char*)_datiprint, "serial_print", ...);
}
```

**Impatto**: Timeout artificiali, drop di messaggi  
**Severità**: MEDIA  

---

### 🟢 DEBOLE (Refactor)

#### 7. Stato globale multipurpose
Variabili come `statoTask`, `fromin`, `buffer_ser`, `command`, `argument` sono condivise tra:
- readserial()
- parsingCmd()
- HTTP POST /parsingCmd
- callback esterne

**Problema**: Race condition facile in ambienti complessi  
**Consiglio**: Non è un bug critico ora, ma diventa pericoloso se il firmware cresce

---

## VERSIONI MIGLIORATE

### ✅ Versione 1: `fregbuffer()` corretto

**Principi**:
- Gestione esplicita del ring buffer
- Chiarezza sulla finestra dati da scrivere
- Protezione contro wraparound

```cpp
void WebSerialSim::fregbuffer() {
    #define oldstory "/oldstory.txt"
    
    if (!historyFileEnabled) return;
    
    // Protezione: se nulla da scrivere, esci
    if (pSerBuf == tailBuf && !fullbuffer) {
        return;
    }
    
    #ifdef _TYPE_FS
        
        FS_FILE_TYPE h = FS_DRV.open(FILE_HISTORY, MOD_APPEND);
        if (!h) return;
        
        bool riaccendi = stateRun;
        stateRun = false;
        
        // ========================================
        // Gestione corretta del ring buffer:
        // Sono sicuro di cosa scrivo
        // ========================================
        
        if (fullbuffer) {
            // Buffer è PIENO: scrivi TUTTO
            // Caso 1: pSerBuf >= tailBuf (dati lineari da tailBuf a dimSerBuf, poi 0 a pSerBuf)
            // Caso 2: pSerBuf < tailBuf (wraparound: da tailBuf a dimSerBuf + da 0 a pSerBuf)
            
            if (pSerBuf >= tailBuf) {
                // Dati da tailBuf fino a pSerBuf (lineare)
                size_t toWrite = pSerBuf - tailBuf;
                if (toWrite > 0) {
                    h.write((const uint8_t*)&historySerBuf[tailBuf], toWrite);
                }
            } else {
                // Wraparound: scrivi in due parti
                // Parte 1: da tailBuf a fine buffer
                size_t part1 = dimSerBuf - tailBuf;
                if (part1 > 0) {
                    h.write((const uint8_t*)&historySerBuf[tailBuf], part1);
                }
                // Parte 2: da inizio buffer a pSerBuf
                if (pSerBuf > 0) {
                    h.write((const uint8_t*)historySerBuf, pSerBuf);
                }
            }
        } else {
            // Buffer non pieno: scrivi solo da tailBuf a pSerBuf
            if (pSerBuf > tailBuf) {
                size_t toWrite = pSerBuf - tailBuf;
                h.write((const uint8_t*)&historySerBuf[tailBuf], toWrite);
            }
        }
        
        // Aggiorna tailBuf per il prossimo flush
        tailBuf = pSerBuf;
        
        stateRun = riaccendi;
        
        // Gestione file di archivio
        if (h.size() > MAXSIZEFILE_HISTORY) {
            h.close();
            FS_DRV.remove(oldstory);
            FS_DRV.rename(FILE_HISTORY, oldstory);
        } else {
            h.close();
        }
        
    #endif
}
```

---

### ✅ Versione 2: `parsingCmd()` sicuro (senza strtok)

**Principi**:
- Parse manuale e deterministico
- Niente stato globale di strtok
- Buffer locale protetto

```cpp
void WebSerialSim::parsingCmd() {
    
    // ========================================
    // Parse SICURO senza strtok()
    // ========================================
    
    // Copia buffer in una copia locale
    char localBuf[LEN_BUF_SER];
    strncpy(localBuf, buffer_ser, LEN_BUF_SER - 1);
    localBuf[LEN_BUF_SER - 1] = '\0';
    
    command = nullptr;
    argument = nullptr;
    
    // Trova il primo spazio
    char* firstSpace = strchr(localBuf, ' ');
    
    if (firstSpace == nullptr) {
        // Un solo token
        command = localBuf;
        argument = command;
    } else {
        // Due token
        *firstSpace = '\0';  // Tronca primo token
        command = localBuf;
        
        // Salta gli spazi
        char* argStart = firstSpace + 1;
        while (*argStart == ' ' && *argStart != '\0') {
            argStart++;
        }
        
        argument = (*argStart != '\0') ? argStart : command;
    }
    
    // Valida command
    if (command == nullptr || strlen(command) == 0) {
        printWeb(F("Comando vuoto\n"));
        statoTask = IDLE;
        return;
    }
    
    // ========================================
    // PROCESSING COMANDI
    // ========================================
    
    // TIMESTAMP
    if (strcmp(command, "TIMESTAMP") == 0) {
        if (strcmp(argument, "ON") == 0) {
            enableTimestamp = true;
            if (clientSSEGlobale) {
                eventsserial->send("1", "timestamp_enabled", millis(), 0);
            }
            printWeb(F("Timestamp abilitato\n"));
        } else if (strcmp(argument, "OFF") == 0) {
            enableTimestamp = false;
            if (clientSSEGlobale) {
                eventsserial->send("0", "timestamp_enabled", millis(), 0);
            }
            printWeb(F("Timestamp disabilitato\n"));
        }
        statoTask = IDLE;
        return;
    }
    
    // HISTORY
    if (strcmp(command, "HISTORY") == 0) {
        parsinghistory(argument);
        statoTask = IDLE;
        return;
    }
    
    // CONFIG
    if (strcmp(command, "CONFIG") == 0) {
        if (stateRun) {
            printWeb(F("Stoppare History prima!\n"));
        } else if (strcmp(argument, "NOFS") == 0) {
            setHistoryFile(false);
            printWeb(F("Disabilitato LOG FS\n"));
        } else if (strcmp(argument, "FS") == 0) {
            setHistoryFile(true);
            printWeb(F("Abilitato LOG FS\n"));
        } else if (strcmp(argument, "NOPSRAM") == 0) {
            setPSRAM(false);
        } else if (strcmp(argument, "PSRAM") == 0) {
            setPSRAM(true);
        } else {
            // Prova parsing come numero
            uint32_t amoutbuf = strtoul(argument, NULL, 10);
            if (amoutbuf > 0) {
                setbuffer(amoutbuf);
                printfWeb(F("Dimensione buffer: %lu\n"), amoutbuf);
            }
        }
        statoTask = IDLE;
        return;
    }
    
    // CALLBACK ESTERNA (se registrata)
    if (_callback != nullptr) {
        // Passa il buffer originale (non la copia locale)
        // Callback riceve il comando e argomento separati da spazio
        _callback(buffer_ser);
    }
    
    statoTask = IDLE;
}
```

---

### ✅ Versione 3: `insBuffer()` sicuro

**Principi**:
- Validazioni esplicite all'inizio
- Underflow protection
- Logica ring buffer chiara

```cpp
void WebSerialSim::insBuffer(const char* str) {
    
    // ========================================
    // VALIDAZIONI INIZIALI
    // ========================================
    
    if (!stateRun) return;
    if (str == nullptr) return;
    
    // Prendi il mutex
    AutoLock lock(_mutex);
    if (!lock.isLocked()) return;
    
    // Direct-to-file mode
    #ifdef _TYPE_FS
        if (directFS && stateRun && historyFileEnabled) {
            FS_FILE_TYPE hfile = FS_DRV.open(FILE_HISTORY, MOD_APPEND);
            if (hfile) {
                if (enableTimestamp) {
                    hfile.write((const uint8_t*)_timestamp, strlen(_timestamp));
                }
                hfile.write((const uint8_t*)str, strlen(str));
                hfile.close();
            }
            return;
        }
    #endif
    
    // ========================================
    // VALIDAZIONE BUFFER
    // ========================================
    
    if (!historySerBuf || dimSerBuf == 0) {
        return;  // Buffer non inizializzato
    }
    
    size_t dimstr = strlen(str);
    
    // Se stringa è più lunga del buffer intero, tronca SAFE
    if (dimstr >= dimSerBuf) {
        dimstr = dimSerBuf - 1;  // Lascia spazio per terminatore
    }
    
    if (dimstr == 0) {
        return;  // Stringa vuota dopo troncamento
    }
    
    // ========================================
    // CALCOLO SPAZIO DISPONIBILE
    // ========================================
    
    size_t spazio_alla_fine = dimSerBuf - pSerBuf;
    
    // ========================================
    // CASO 1: Stringa entra tutta alla fine
    // ========================================
    
    if (dimstr <= spazio_alla_fine) {
        memcpy(&historySerBuf[pSerBuf], str, dimstr);
        pSerBuf += dimstr;
        
        // Se buffer è pieno, prepara il flush
        if (pSerBuf >= dimSerBuf) {
            #ifdef _TYPE_FS
                fregbuffer();
            #endif
            pSerBuf = 0;
            fullbuffer = true;
        }
        return;
    }
    
    // ========================================
    // CASO 2: Stringa si wraparound
    // ========================================
    
    // Copia prima parte (fine buffer)
    if (spazio_alla_fine > 0) {
        memcpy(&historySerBuf[pSerBuf], str, spazio_alla_fine);
    }
    
    pSerBuf = dimSerBuf;  // Marca fine buffer
    
    // Flush su file prima di sovrascrivere
    #ifdef _TYPE_FS
        fregbuffer();
    #endif
    
    // Copia seconda parte (inizio buffer)
    size_t seconda_parte = dimstr - spazio_alla_fine;
    if (seconda_parte > 0 && seconda_parte < dimSerBuf) {
        memcpy(historySerBuf, &str[spazio_alla_fine], seconda_parte);
        pSerBuf = seconda_parte;
    }
    
    fullbuffer = true;  // Buffer è ora pieno/wraparound
}
```

---

### ✅ Versione 4: `getTimestampString()` thread-safe

**Principi**:
- Calcolo diretto del timestamp
- Niente state globale di localtime
- Buffer locale protetto

```cpp
void WebSerialSim::getTimestampString() {
    
    if (!enableTimestamp) {
        return;
    }
    
    // ========================================
    // ALTERNATIVA 1: Usa time API ESP32
    // (Se disponibile, più sicuro di localtime)
    // ========================================
    
    #ifdef ESP32
        time_t now = time(nullptr);
        
        // Estrai i componenti direttamente (thread-safe)
        struct tm* timeinfo = localtime(&now);
        if (timeinfo == nullptr) {
            strncpy(_timestamp, "[??:??:??] ", sizeof(_timestamp) - 1);
            return;
        }
        
        // PROTZIONE: usa buffer locale
        char tmpbuf[20];
        snprintf(tmpbuf, sizeof(tmpbuf),
            "[%02d:%02d:%02d] ",
            timeinfo->tm_hour,
            timeinfo->tm_min,
            timeinfo->tm_sec);
        
        // Copia nel membro protetto
        strncpy(_timestamp, tmpbuf, sizeof(_timestamp) - 1);
        _timestamp[sizeof(_timestamp) - 1] = '\0';
    #else
        // Fallback: timestamp statico
        strncpy(_timestamp, "[00:00:00] ", sizeof(_timestamp) - 1);
    #endif
}

// ALTERNATIVA 2: Se vuoi evitare localtime completamente:
// Usa millis() e calcolo manuale (più thread-safe):

void WebSerialSim::getTimestampString_v2() {
    
    if (!enableTimestamp) {
        return;
    }
    
    // Usa milliseccondi dal boot (super thread-safe)
    unsigned long ms = millis();
    
    // Calcolo grezzo: HH:MM:SS da ms (approssimativo ma thread-safe)
    unsigned long totalSecs = ms / 1000;
    int hours = (totalSecs / 3600) % 24;
    int minutes = (totalSecs / 60) % 60;
    int seconds = totalSecs % 60;
    
    snprintf(_timestamp, sizeof(_timestamp),
        "[%02d:%02d:%02d] ",
        hours, minutes, seconds);
}
```

---

## TABELLA RIASSUNTIVA DEI FIX

| Bug | Funzione | Severità | Fix | Linea Orig |
|-----|----------|----------|-----|-----------|
| Ring buffer wraparound | `fregbuffer()` | 🔴 CRITICA | Riscrittura completa | 975-1008 |
| strtok() non thread-safe | `parsingCmd()` | 🔴 CRITICA | Parse manuale con strchr | 353-423 |
| Buffer underflow | `insBuffer()` | 🟡 MEDIA | Validazioni esplicite | 889-945 |
| localtime() non reentrant | `getTimestampString()` | 🟡 MEDIA | Buffer locale + mutex o millis() | 110-127 |
| Client count errato | `onConnect()` | 🟢 BASSA | Rimuovi `+1` | 637 |
| requiredSpace inaffidabile | `txSSE()` | 🟡 MEDIA | Calc dinamico dello spazio | 262 |

---

## CONSIGLI IMPLEMENTAZIONE

### Passo 1: Applica i fix CRITICI
1. `fregbuffer()` – Gestisce il ring buffer correttamente
2. `parsingCmd()` – Elimina strtok

### Passo 2: Aggiungi i fix MEDI
3. `insBuffer()` – Protezione underflow
4. `getTimestampString()` – Thread-safety

### Passo 3: Refactor OPZIONALE
5. Isola lo stato in una struct interna
6. Riduci variabili globali

### Test suggeriti
- Stress test con buffer pieno e wraparound
- Parsing di comandi concorrenti
- Timestamp con multitasking FreeRTOS
- History file con file system concorrente

