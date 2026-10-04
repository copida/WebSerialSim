# WebSerialSim

**WebSerialSim** è una libreria per ESP32 / Arduino IDE che trasforma un dispositivo ESP32 in un semplice e completo **terminale seriale accessibile dal browser**.

La comunicazione tra ESP32 e browser utilizza **Server-Sent Events (SSE)** per trasferire l'output in tempo reale.

La libreria nasce con un obiettivo preciso:

> **essere leggera, essenziale e funzionale, senza rinunciare agli strumenti utili per il monitoraggio e la diagnosi del dispositivo.**

Oltre al terminale web, WebSerialSim dispone di un sistema **History** configurabile che può utilizzare memoria SRAM, PSRAM oppure scrivere direttamente su filesystem.

---

## Caratteristiche

* Terminale web basato su **SSE (Server-Sent Events)**
* Compatibile con `Serial`, `print()`, `println()` e `printf()`
* Callback per la gestione dei comandi ricevuti dal terminale
* Callback opzionale per BLE o altre sorgenti
* Timestamp opzionale
* Echo opzionale dell'output sulla seriale
* History su buffer RAM
* History su **PSRAM**
* History su SRAM interna come fallback
* History direttamente su filesystem
* Buffer History configurabile
* Possibilità di disabilitare completamente il buffer
* Possibilità di disabilitare la History su file
* Visualizzazione della History dal browser
* Download della History
* Cancellazione del file History
* Comandi di configurazione direttamente dal terminale
* Supporto a HTML interno oppure HTML esterno
* Possibilità di utilizzare HTML compresso GZIP
* Utilizzabile anche come strumento di diagnostica dopo un riavvio o un errore del dispositivo

---

# Come funziona

Una volta inizializzata la libreria:

```cpp
#include "WebSerialSim.h"

WebSerialSim serialWeb;
```

nel `setup()`:

```cpp
serialWeb.begin(&server);
```

e nel `loop()`:

```cpp
serialWeb.taskList();
```

l'ESP32 dispone di un terminale web raggiungibile tramite:

```text
http://INDIRIZZO_IP_ESP32/serial
```

L'output prodotto tramite:

```cpp
serialWeb.print("Hello");
serialWeb.println("Hello");
serialWeb.printf("Valore: %d\n", valore);
```

può essere visualizzato in tempo reale dal browser.

L'uso della libreria è quindi molto simile all'utilizzo della normale `Serial`.

---

# Esempio minimo

```cpp
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

#include "WebSerialSim.h"

WebSerialSim serialWeb;

void setup() {

    Serial.begin(115200);

    WiFi.begin("SSID", "PASSWORD");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    serialWeb.begin(&server);

    server.begin();
}

void loop() {

    serialWeb.taskList();

    serialWeb.println("Messaggio dal WebSerialSim");

    delay(1000);
}
```

---

# Un esempio più completo

Il progetto comprende un esempio `.ino` che mostra:

* connessione Wi-Fi;
* inizializzazione SD;
* verifica PSRAM;
* callback;
* configurazione History;
* uso del terminale;
* comandi applicativi;
* produzione periodica di dati;
* utilizzo di `print()`, `println()` e `printf()`.

Esempio di callback:

```cpp
void onCbReceive(char* cmd) {

    serialWeb.println(cmd);

    if (strstr(cmd, "ON") != NULL) {

        digitalWrite(LED_BUILTIN, HIGH);
        serialWeb.print("LED ACCESO\n");

    } else if (strstr(cmd, "OFF") != NULL) {

        digitalWrite(LED_BUILTIN, LOW);
        serialWeb.print("LED SPENTO\n");

    } else if (strstr(cmd, "DIR") != NULL) {

        serialWeb.print("Eseguo DIR\n");
        listDir();
    }
}
```

La callback permette quindi di utilizzare il terminale web anche come interfaccia di comando per il programma.

---

# History

La funzione History è uno degli elementi principali di WebSerialSim.

La History può essere configurata indipendentemente dal terminale e può utilizzare:

* buffer SRAM;
* buffer PSRAM;
* nessun buffer;
* filesystem.

Il buffer funziona come una memoria circolare.

Quando il buffer raggiunge la sua capacità, i dati più vecchi vengono progressivamente sostituiti dai nuovi.

Se è abilitato il filesystem, i dati del buffer possono essere scaricati nel file History prima della sovrascrittura.

Questo permette di utilizzare contemporaneamente memoria RAM/PSRAM e memoria permanente.

---

# Le quattro configurazioni principali

## 1. Buffer + History file

Configurazione predefinita.

```cpp
serialWeb.setCallback(onCbReceive);

serialWeb.begin(&server);
```

In questa modalità viene utilizzato il buffer History e, quando abilitato, i dati vengono registrati anche nel file History.

È la configurazione più completa.

---

## 2. Nessun buffer, nessun file

```cpp
serialWeb.setbuffer(0);
serialWeb.setHistoryFile(false);

serialWeb.begin(&server);
```

Questa modalità disabilita completamente la registrazione della History.

Il terminale continua comunque a funzionare normalmente.

È utile quando si vuole utilizzare WebSerialSim esclusivamente come terminale web.

---

## 3. Nessun buffer, History direttamente su file

```cpp
serialWeb.setbuffer(0);
serialWeb.setHistoryFile(true);

serialWeb.begin(&server);
```

Con buffer impostato a `0`, se la History su filesystem è abilitata, i dati possono essere scritti direttamente nel file History.

In questo modo non viene occupata memoria RAM/PSRAM per il buffer.

Questa modalità può essere particolarmente interessante per il monitoraggio di sistemi nei quali si vuole conservare lo storico senza dedicare una grande quantità di RAM alla History.

---

## 4. Solo buffer

```cpp
serialWeb.setbuffer(1048576);
serialWeb.setHistoryFile(false);

serialWeb.begin(&server);
```

In questo caso viene utilizzato un buffer da 1 MB senza registrazione su file.

È possibile utilizzare questa configurazione, ad esempio, per conservare in memoria una quantità consistente di output senza effettuare scritture sul filesystem.

---

# Buffer SRAM / PSRAM

La dimensione del buffer può essere configurata tramite:

```cpp
serialWeb.setbuffer(1048576);
```

La dimensione è espressa in byte.

Esempi:

```cpp
serialWeb.setbuffer(4000);
```

```cpp
serialWeb.setbuffer(100000);
```

```cpp
serialWeb.setbuffer(1048576);
```

Per utilizzare la PSRAM:

```cpp
serialWeb.setPSRAM(true);
```

Per utilizzare la SRAM interna:

```cpp
serialWeb.setPSRAM(false);
```

Quando la PSRAM è richiesta ma non disponibile o non è possibile effettuare l'allocazione, la libreria può utilizzare la SRAM interna come fallback.

Se anche l'allocazione alternativa non è possibile, la History viene disabilitata, mentre il terminale WebSerialSim può continuare a funzionare.

Questo comportamento permette di utilizzare la libreria anche in situazioni nelle quali la configurazione della memoria non è quella prevista.

---

# Diagnostica della History

Dal terminale è possibile richiedere informazioni sullo stato della History:

```text
HISTORY INFO
```

Le informazioni comprendono, quando disponibili:

* presenza del buffer;
* dimensione del buffer;
* quantità di memoria utilizzata;
* tipo di memoria utilizzata;
* stato della registrazione;
* stato del file History;
* eventuale modalità di scrittura diretta su file.

La funzione di diagnostica è particolarmente utile quando il dispositivo deve continuare a funzionare anche in presenza di problemi di allocazione della memoria.

---

# Configurazione tramite terminale

Molte impostazioni possono essere modificate direttamente dal terminale web o dalla porta seriale.

## Dimensione buffer

```text
CONFIG 5000
```

imposta un buffer di 5000 byte.

Per esempio:

```text
CONFIG 100000
```

imposta un buffer di 100000 byte.

---

## Filesystem

Disabilita la registrazione su file:

```text
CONFIG NOFS
```

Abilita la registrazione su file:

```text
CONFIG FS
```

---

## PSRAM

Disabilita l'utilizzo della PSRAM:

```text
CONFIG NOPSRAM
```

Abilita l'utilizzo della PSRAM:

```text
CONFIG PSRAM
```

---

## Timestamp

Abilita il timestamp:

```text
TIMESTAMP ON
```

Disabilita il timestamp:

```text
TIMESTAMP OFF
```

Il timestamp viene utilizzato nel formato:

```text
[HH:MM:SS]
```

e può essere visualizzato sia nel terminale sia nella History.

---

# Comandi History

I comandi principali disponibili sono:

```text
HISTORY ON
```

Attiva la History.

```text
HISTORY OFF
```

Disattiva la History.

```text
HISTORY FLUSH
```

Scarica sul filesystem i dati presenti nel buffer, quando il filesystem è abilitato.

```text
HISTORY INFO
```

Visualizza le informazioni sullo stato della History.

---

# Attenzione alla configurazione

Le impostazioni relative alla dimensione del buffer e al filesystem possono essere modificate tramite `CONFIG`.

Quando la History è attiva, alcune modifiche di configurazione richiedono prima di arrestare la registrazione.

Ad esempio:

```text
HISTORY OFF
CONFIG 1000000
HISTORY ON
```

Questo permette di modificare la configurazione del buffer in modo esplicito.

---

# File History

Il file utilizzato dalla libreria è:

```text
/history.txt
```

La dimensione massima prevista per il file è configurabile tramite:

```cpp
#define MAXSIZEFILE_HISTORY 512000
```

Quando viene raggiunta la dimensione prevista, la libreria gestisce la rotazione dello storico utilizzando un secondo file.

Il file può quindi essere utilizzato anche come storico persistente dell'attività del dispositivo.

---

# Visualizzazione e download

La pagina web permette di accedere alla History attraverso le funzioni dedicate.

È possibile:

* visualizzare lo storico;
* scaricare `history.txt`;
* cancellare il file History.

Prima della visualizzazione o del download, la libreria sincronizza il contenuto del buffer con il filesystem quando necessario.

Questo permette di avere una rappresentazione aggiornata dello storico.

---

# Server-Sent Events

La comunicazione in tempo reale utilizza la tecnologia **Server-Sent Events (SSE)**.

Il browser mantiene una connessione persistente con:

```text
/events/serial
```

e riceve gli eventi prodotti dall'ESP32.

L'utilizzo di SSE è particolarmente adatto a WebSerialSim perché il flusso principale dei dati è:

```text
ESP32  --->  Browser
```

senza la necessità di utilizzare WebSocket per la normale trasmissione dell'output.

---

# Trasmissione di grandi quantità di dati

WebSerialSim suddivide automaticamente i dati di grandi dimensioni in blocchi adatti alla trasmissione SSE.

La dimensione di riferimento è:

```cpp
#define CHUNK_SIZE 1440
```

La gestione dei blocchi cerca di mantenere, quando possibile, la separazione naturale delle righe.

Il sistema gestisce anche dati costituiti da blocchi molto grandi e linee particolarmente lunghe.

Per applicazioni che generano grandi quantità di output è comunque consigliabile utilizzare una normale terminazione delle righe con `\n`.

---

# Più browser collegati

WebSerialSim non nasce come sistema multi-client complesso con una gestione indipendente di ogni sessione.

È però possibile avere più browser collegati contemporaneamente alla sorgente SSE.

Gli eventi SSE possono essere visualizzati dalle diverse sessioni.

La libreria mantiene inoltre un client SSE di riferimento utilizzato per la gestione del flusso e del controllo della disponibilità della connessione.

Quando una nuova sessione diventa il client di riferimento, la precedente sessione può ricevere una notifica del cambio.

Questa scelta mantiene l'implementazione semplice e leggera, pur permettendo un utilizzo pratico con più finestre/browser.

---

# Controllo del flusso SSE

La trasmissione non viene effettuata indiscriminatamente alla massima velocità possibile.

Prima dell'invio vengono controllate le condizioni del socket TCP, compresa la quantità di spazio disponibile e la possibilità effettiva di trasmettere.

In caso di attesa, la libreria lascia tempo allo stack di rete di procedere.

È inoltre presente un timeout per evitare che una connessione non più in grado di trasmettere blocchi indefinitamente il programma.

Questo meccanismo è particolarmente importante durante l'invio di grandi quantità di dati.

---

# Output sulla Serial

L'output destinato al browser può essere contemporaneamente inviato sulla normale porta `Serial`.

È possibile abilitare l'echo tramite:

```cpp
serialWeb.echoOnOff(true);
```

e disabilitarlo con:

```cpp
serialWeb.echoOnOff(false);
```

Questo può essere utile durante il debug, quando si vuole osservare contemporaneamente il terminale hardware e quello web.

---

# Callback

La callback principale viene registrata con:

```cpp
serialWeb.setCallback(onCbReceive);
```

La funzione deve avere la forma:

```cpp
void onCbReceive(char* cmd)
{
    // gestione comando
}
```

La callback riceve il comando proveniente dal terminale.

Questo permette di utilizzare WebSerialSim non soltanto come monitor, ma anche come semplice interfaccia di comando per il programma.

---

# Callback BLE / altra sorgente

È disponibile anche una seconda callback:

```cpp
serialWeb.setCallBLE(callback);
```

che può essere utilizzata per inoltrare dati verso BLE o verso un'altra sorgente prevista dall'applicazione.

L'utilizzo è opzionale.

---

# HTML interno o esterno

Una caratteristica importante di WebSerialSim è la possibilità di scegliere se utilizzare la pagina HTML incorporata nella libreria oppure fornire la propria pagina web.

## HTML interno

Abilitando:

```cpp
#define INTERNALHTML
```

la libreria può fornire direttamente la pagina del terminale tramite:

```text
/serial
```

In questo modo il dispositivo è autonomo e non necessita di un file HTML esterno.

---

## HTML esterno

Se `INTERNALHTML` non è definito, la libreria continua a fornire le API e il servizio SSE, ma la pagina HTML può essere ospitata esternamente.

Questa modalità è particolarmente utile quando si vuole:

* ridurre l'occupazione di memoria del programma ESP32;
* sviluppare/modificare la pagina web indipendentemente dalla libreria;
* utilizzare una pagina HTML personalizzata.

La modalità esterna abilita inoltre gli header CORS necessari per consentire il collegamento da un'origine differente.

---

# HTML compresso GZIP

Quando viene utilizzato l'HTML interno è possibile utilizzare una versione compressa GZIP della pagina.

La configurazione è:

```cpp
#define INTERNALHTML
#define GZIP
```

La pagina compressa viene trasferita con:

```text
Content-Encoding: gzip
```

Il browser provvede automaticamente alla decompressione.

Questa modalità permette di ridurre significativamente la quantità di dati necessari per trasferire la pagina HTML e può essere utile anche per ridurre l'impatto della pagina incorporata nel progetto.

---

# API principale

## Costruttore

```cpp
WebSerialSim serialWeb;
```

Crea un'istanza della libreria.

---

## `begin()`

```cpp
serialWeb.begin(&server);
```

Inizializza WebSerialSim e registra nel server le route utilizzate dalla libreria.

Il server viene fornito dallo sketch:

```cpp
AsyncWebServer server(80);
```

e viene successivamente avviato normalmente:

```cpp
server.begin();
```

---

## `taskList()`

```cpp
serialWeb.taskList();
```

Deve essere richiamata nel `loop()`.

Gestisce:

* ricezione dei comandi;
* parsing;
* timeout dell'input;
* attività della libreria.

Esempio:

```cpp
void loop()
{
    serialWeb.taskList();

    // resto del programma
}
```

---

## `setbuffer()`

```cpp
serialWeb.setbuffer(size);
```

Imposta la dimensione del buffer History.

Esempio:

```cpp
serialWeb.setbuffer(1000000);
```

Per disabilitare il buffer:

```cpp
serialWeb.setbuffer(0);
```

---

## `setHistoryFile()`

```cpp
serialWeb.setHistoryFile(true);
```

abilita la registrazione su file.

```cpp
serialWeb.setHistoryFile(false);
```

la disabilita.

---

## `setPSRAM()`

```cpp
serialWeb.setPSRAM(true);
```

abilita l'utilizzo della PSRAM per il buffer.

```cpp
serialWeb.setPSRAM(false);
```

disabilita l'utilizzo della PSRAM.

---

## `setTimestamp()`

```cpp
serialWeb.setTimestamp(true);
```

abilita il timestamp.

```cpp
serialWeb.setTimestamp(false);
```

lo disabilita.

---

## `echoOnOff()`

```cpp
serialWeb.echoOnOff(true);
```

abilita l'echo sulla Serial.

---

## `setCallback()`

```cpp
serialWeb.setCallback(onCbReceive);
```

registra la callback per i comandi.

---

## `setCallBLE()`

```cpp
serialWeb.setCallBLE(callbackBLE);
```

registra la callback opzionale per BLE o altra sorgente.

---

## `playstory()`

Permette di avviare o arrestare la registrazione della History.

```cpp
serialWeb.playstory(true);
```

avvia.

```cpp
serialWeb.playstory(false);
```

arresta.

---

## `infoSerBuf()`

```cpp
serialWeb.infoSerBuf();
```

visualizza le informazioni relative al buffer e alla History.

---

# Utilizzo con `Print`

`WebSerialSim` deriva dalla classe Arduino `Print`.

Di conseguenza è possibile utilizzare le normali funzioni:

```cpp
serialWeb.print("Test");

serialWeb.println("Test");

serialWeb.printf("Temperatura: %.2f\n", temperatura);
```

È inoltre possibile utilizzare `write()` quando è necessario trasmettere dati binari o buffer di caratteri.

---

# Un esempio reale

Una tipica applicazione può essere strutturata così:

```cpp
void setup()
{
    Serial.begin(115200);

    // connessione WiFi...

    serialWeb.setCallback(onCbReceive);

    serialWeb.setbuffer(100000);
    serialWeb.setHistoryFile(true);
    serialWeb.setPSRAM(true);
    serialWeb.setTimestamp(true);

    serialWeb.begin(&server);

    server.begin();
}

void loop()
{
    serialWeb.taskList();

    serialWeb.printf("Sistema attivo: %lu ms\n", millis());

    delay(1000);
}
```

Il programma può continuare a utilizzare normalmente:

```cpp
Serial
WiFi
SD
BLE
sensori
altri task
```

mentre WebSerialSim si occupa del terminale web e della relativa History.

---

# Configurazione di compilazione

Le principali opzioni della libreria vengono definite nell'header.

Esempio:

```cpp
#define MAXSIZEBUFFER_HISTORY 4000
#define CHUNK_SIZE 1440

#define FILE_HISTORY "/history.txt"
#define MAXSIZEFILE_HISTORY 512000
```

È inoltre possibile selezionare il filesystem previsto dalla configurazione della scheda/progetto.

La configurazione del filesystem deve essere coerente con quello effettivamente utilizzato dallo sketch.

---

# Quando utilizzare le diverse modalità

### Terminale puro

```cpp
setbuffer(0);
setHistoryFile(false);
```

Utilizzare quando serve soltanto il monitor web.

### Monitoraggio con storico persistente

```cpp
setbuffer(0);
setHistoryFile(true);
```

Utile quando si vuole conservare l'output su filesystem senza dedicare memoria al buffer.

### Buffer di grandi dimensioni

```cpp
setbuffer(1048576);
setHistoryFile(false);
```

Utile quando si vuole mantenere molto output in RAM/PSRAM.

### Configurazione completa

```cpp
setbuffer(4000);
setHistoryFile(true);
setPSRAM(true);
```

Utile quando si desidera combinare buffer circolare e storico persistente.

---

# Diagnostica dopo un riavvio

Uno degli utilizzi interessanti di WebSerialSim è il monitoraggio di dispositivi che possono operare per lunghi periodi senza supervisione.

In caso di riavvio, il sistema può comunque avviare il terminale web anche se la configurazione della History non è riuscita a creare il buffer richiesto.

Questo permette di collegarsi nuovamente tramite browser e utilizzare le informazioni disponibili per verificare:

* stato della History;
* dimensione richiesta;
* memoria utilizzata;
* presenza della PSRAM;
* stato del filesystem;
* eventuali messaggi prodotti durante l'inizializzazione.

La History non deve quindi necessariamente impedire l'avvio del terminale principale.

---

# Filosofia del progetto

WebSerialSim non vuole essere un framework web completo e non vuole trasformare l'ESP32 in un sistema complesso di gestione delle sessioni.

Il progetto segue alcuni principi semplici:

* poche dipendenze;
* API semplici;
* utilizzo naturale delle funzioni `Print`;
* niente gestione complessa dei client quando non necessaria;
* uso della PSRAM quando disponibile;
* possibilità di funzionare anche senza PSRAM;
* History opzionale;
* filesystem opzionale;
* HTML opzionale;
* attenzione all'utilizzo della memoria;
* possibilità di utilizzare il terminale come strumento di diagnostica.

L'obiettivo è avere uno strumento che possa essere aggiunto a un progetto ESP32 senza modificarne radicalmente l'architettura.

---

# Note sul comportamento SSE

L'output SSE viene trasmesso in blocchi per evitare di inviare grandi quantità di dati in un'unica operazione.

Il sistema controlla la disponibilità del buffer TCP prima dell'invio e può attendere temporaneamente che il socket torni disponibile.

Questo comportamento è importante soprattutto quando il programma deve inviare file o grandi quantità di testo al browser.

La pagina web riceve gli eventi e aggiorna progressivamente il terminale senza dover ricaricare la pagina.

---

# Limiti e considerazioni

La dimensione del buffer History deve essere scelta in funzione della memoria disponibile.

Un buffer molto grande può occupare una quantità significativa di PSRAM o SRAM.

Se la PSRAM è disponibile, è generalmente preferibile utilizzarla per buffer di grandi dimensioni.

Il filesystem deve essere inizializzato correttamente dallo sketch prima di utilizzare le funzioni che richiedono la History su file.

La libreria non sostituisce il normale sistema di logging del programma: offre un terminale web e un sistema History integrato pensato principalmente per monitoraggio, debug e diagnostica.

---

# Stato del progetto

WebSerialSim è un progetto in evoluzione.

La struttura attuale è stata sviluppata e verificata attraverso prove di trasmissione SSE con quantità consistenti di dati, gestione di file, buffer History, PSRAM e più sessioni browser.

Le configurazioni e le API descritte in questo documento si riferiscono alla versione corrente della libreria.

Nuove ottimizzazioni o modifiche potranno essere introdotte nelle versioni successive senza cambiare necessariamente la filosofia generale del progetto.

---

## In breve

Con poche righe:

```cpp
WebSerialSim serialWeb;

void setup()
{
    serialWeb.begin(&server);
    server.begin();
}

void loop()
{
    serialWeb.taskList();
}
```

si ottiene un terminale web SSE per ESP32.

A seconda della configurazione è poi possibile aggiungere:

```text
        ┌───────────────────────┐
        │     WebSerialSim      │
        ├───────────────────────┤
        │      Web Terminal     │
        │          SSE          │
        ├───────────────────────┤
        │   Buffer SRAM/PSRAM   │
        │      History          │
        │   Filesystem History  │
        ├───────────────────────┤
        │ Callback / BLE /      │
        │ Timestamp / Echo      │
        └───────────────────────┘
```

**Un terminale web semplice, ma con gli strumenti necessari per trasformarlo anche in un piccolo sistema di monitoraggio e diagnostica per ESP32.**

