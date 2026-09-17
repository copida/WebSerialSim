#include "WebSerialSim.h"
#include <stdarg.h>
#include <time.h>

// Seriale ─┐
// Web HTTP ├──> coda comandi ───> taskList() ───> parser/callback
// BLE ─────┘

// ======================
// COSTRUTTORE
// ======================
WebSerialSim::WebSerialSim() {
	
	server = nullptr;
	eventsserial = nullptr;
	
	#ifdef _TYPE_FS
	historyFileEnabled = true;
	#else
	historyFileEnabled = false;
	#endif
	historySerBuf = nullptr;
	stateRun = false;
	pSerBuf = 0;
	tailBuf = 0;
	dimSerBuf = 0;
	tmpdimSerBuf = MAXSIZEBUFFER_HISTORY;
	fullbuffer = false;
	
	inPSRAM = true;
		
	bufIndexIn = 0;		//
	
	command = nullptr;
	argument = nullptr;
	
	echon = false;
	
	fromin = FROMWEB;
	statoTask = IDLE;
	
	_callback = nullptr;
	_callBLE = nullptr;
	
	//targetClient = nullptr;
	clientSSEGlobale = nullptr;
	
}

void WebSerialSim::handleBufferIn() {
	if (bufIndexIn == 0) return;
	
	if (millis() - ultimoCarattereTime >= TIMEOUT_MS) {
		bufferIn[bufIndexIn] = '\0'; // Sicurezza: garantisci il terminatore prima di stampare
		printWeb(bufferIn);
		bufIndexIn = 0;
		bufferIn[0] = 0;
	}
}

void WebSerialSim::printWeb(char* _datiprint, size_t quantsize) {
	if (quantsize == 0) quantsize = strlen(_datiprint);
	// 1430 byte è perfetto per lasciare spazio ai metadata SSE nel frame TCP
	if (quantsize > CHUNK_SIZE) {
		printBigBuf(_datiprint, quantsize);
    } else {
		sendWeb(_datiprint, quantsize);
		delay(1);
		//yield();
	}
}


void WebSerialSim::sendWeb(char* _dati, size_t len) {
	
	if (enableTimestamp) getTimestampString();
	// 1. Echo su Serial (se abilitato)
	if (fromin == FROMSER || echon) {
		if (enableTimestamp) Serial.print(_timestamp);
		Serial.write(_dati, len);
	}
	
	if (fromin == FROMWEB && clientSSEGlobale) {
		
		if (enableTimestamp && canSendSSE(11+len)) {			
			eventsserial->send(_timestamp, "timestamp", millis(), 0);
		}
		
		int tentativi = 0;
		const int MAX_TENTATIVI = 20;
		bool invioRiuscito = false;
		
		while (tentativi < MAX_TENTATIVI) {
			
			if (canSendSSE(len)) {
				
				invioRiuscito = eventsserial->send((const char*)_dati, "serial_print", millis(), 0);
				
				if (invioRiuscito) {
					break; // Successo
				}
				
				// TCP pronto ..coda SSE non pronta (32 messaggi)
				tentativi++;
				//delay(2); 
				delay(tentativi); 
				
				} else {
				// l'hardware è completamente bloccato o il client è sparito.
				break; 
			}
		}
		
		if(tentativi > 0) Serial.println(tentativi);
		if(!invioRiuscito) Serial.write(_dati, len);
	}
	
	#ifdef OUTBLE
		if (fromin == FROMBT) if(_callBLE)_callBLE(_dati);
	#endif
	
	// 3. Accumulo diretto nel buffer di cronologia (PSRAM o SRAM)
	if (stateRun) {
		if (enableTimestamp) {
			insHistory(_timestamp);
		}
		insHistory(_dati);
	}
	
}

// ===== TIMESTAMP HELPER =====
char* WebSerialSim::getTimestampString() {
	//static char timestamp[16];  // "[HH:MM:SS] " = 11 char max
	
	if (!enableTimestamp) {
		return (char*)"";  // Ritorna stringa vuota se disabilitato
	}
	
	time_t now = time(nullptr);
	struct tm* timeinfo = localtime(&now);
	
	snprintf(_timestamp, sizeof(_timestamp),
		"[%02d:%02d:%02d] ",
		timeinfo->tm_hour,
		timeinfo->tm_min,
	timeinfo->tm_sec);
	
	return _timestamp;
} 

//======================
// size_t WebSerialSim::write(uint8_t c):
// size_t WebSerialSim::write(const uint8_t *buffer, size_t size):
//======================
size_t WebSerialSim::write(uint8_t m) {
	
	return write(&m, 1);
	//write(&m, 1);
	//return (1);
}

size_t WebSerialSim::write(const uint8_t *buffer, size_t size) {
	if (size == 0 || buffer == nullptr) return 0;
	
	// Se il blocco in arrivo non sta nel buffer rimasto, svuota prima il buffer attuale
	if (size + bufIndexIn >= DIMBUFFERIN - 1 && bufIndexIn != 0) {
		bufferIn[bufIndexIn] = '\0';
		bufIndexIn = 0;
		printWeb(bufferIn);
		//bufIndexIn = 0;
	}
	
	// Se il blocco singolo è più grande dell'intero buffer vuoto, bypassa l'accumulo
	if (size >= DIMBUFFERIN - 1) {
		printBigBuf((char*)buffer, size);
		return size;
	}
	
	memcpy(&bufferIn[bufIndexIn], buffer, size);
	bufIndexIn += size;
	bufferIn[bufIndexIn] = '\0'; // Terminatore sicuro
	
	if (bufferIn[bufIndexIn - 1] == '\n') {
		bufIndexIn = 0;
		printWeb(bufferIn);
		} else {
		ultimoCarattereTime = millis();
	}
	
	return size;
}

// ======================
// printfWeb
// ======================
void WebSerialSim::printfWeb(const char* format, ...) {
	char loc_buf[PRINTF_LENMAX];
	va_list arg;
	va_start(arg, format);
	vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
	va_end(arg);
	
	printWeb(loc_buf);
}

// 2. NUOVA: Funzione di supporto per gestire la macro F()
/* 	void WebSerialSim::printfWeb(const __FlashStringHelper* formato, ...) {
	// Converte il puntatore Flash in un puntatore a caratteri leggibile
	const char* formatoInFlash = (const char*)formato;
	
	char loc_buf[PRINTF_LENMAX];
	
	va_list argomenti;
	va_start(argomenti, formato);
	vsnprintf_P(loc_buf, sizeof(loc_buf), formatoInFlash, argomenti);
	va_end(argomenti);
	
	this->printfWeb("%s", loc_buf);
} */

void WebSerialSim::switchState(int fasestate){
	statoTask = fasestate;
}

// ver NEW ===========================
void WebSerialSim::printBigBuf(char *bigbuf, size_t dim) {
	if (bigbuf == NULL) return;
	if (dim == 0) dim = strlen(bigbuf);
	if (dim == 0) return;
	
	if (dim < CHUNK_SIZE) {
		sendWeb(bigbuf, dim);
		return;
	}
	
	size_t srcIndex = 0;
	
	while (srcIndex < dim) {
		size_t remaining = dim - srcIndex;
		size_t maxToRead = (remaining < CHUNK_SIZE - 1) ? remaining : (CHUNK_SIZE - 1);
		
		int lastLfOffset = -1;
		for (size_t i = 0; i < maxToRead; i++) {
			if (bigbuf[srcIndex + i] == '\n') lastLfOffset = (int)i;
		}
		
		size_t sendLen = 0;
		if (lastLfOffset != -1 && (srcIndex + maxToRead) < dim) {
			sendLen = lastLfOffset + 1;
			} else {
			sendLen = maxToRead;
		}
		
		memcpy(chunkBuf, &bigbuf[srcIndex], sendLen);
		chunkBuf[sendLen] = '\0';
		//delay(2);
		sendWeb(chunkBuf, sendLen);
		srcIndex += sendLen;
	}
	// end while
}

// ======================
// SSE
// ======================
bool WebSerialSim::checkClientSSE() {
	return (eventsserial->count() > 0);
}

bool WebSerialSim::canSendSSE(size_t requiredSpace) {
	
	//if (eventsserial->count() == 0)	return false;
	
	if (eventsserial->count() == 0 || !clientSSEGlobale->connected()) {
		clientSSEGlobale = nullptr;
		Serial.println(F("Client disconnesso!"));
		//clientSSEGlobale->send("[SSE] Client Disconesso\n", "serial_print", millis(), 0);
		return false;
	}
	
	uint32_t startWait = millis();
	
	while (clientSSEGlobale &&
		(clientSSEGlobale->client()->space() < (requiredSpace + 64) ||
		!clientSSEGlobale->client()->canSend())) {
		
		if (eventsserial->count() == 0 || !clientSSEGlobale->connected()) {
			clientSSEGlobale = nullptr;
			Serial.println(F("Client disconnesso!"));
			return false;
		}
		
		if (millis() - startWait > 2000) {
			Serial.println(F("[SSE] Timeout buffer"));
			return false;
		}
		
		delay(1);
		//yield();
	}
	
	return true;
}

// ======================
// TASK
// ======================
void WebSerialSim::taskList() {
	
	handleBufferIn();
	
	if (statoTask == PARSING)
	parsingCmd();
	else
	readserial(buffer_ser, LEN_BUF_SER);
}

void WebSerialSim::readserial(char* buffer_ch, int max_ch) {
	
	static int p_buf = 0;
	while (Serial.available()) {
		char c = Serial.read();
		
		if (c == '\n') {
			buffer_ch[p_buf] = '\0';
			
			if (statoTask != IDLE) {
				Serial.println(F("Sistema occupato"));
				p_buf = 0;
				return;
				} else if (p_buf > 0) {
				fromin = FROMSER;
				statoTask = PARSING;
				p_buf = 0;
				return; // parsing nel prossimo taskList()
			}
			
			p_buf = 0;
			} else if (c != '\r' && p_buf < max_ch - 1) {
			buffer_ch[p_buf++] = c;
		}
	}
	
}

bool WebSerialSim::inputEXT(char* inExt, int lenb) {
	
	if (statoTask != IDLE)
	return false;
	if(lenb > LEN_BUF_SER) lenb = LEN_BUF_SER;
	memcpy(buffer_ser, inExt, lenb);
	statoTask = PARSING;
	fromin = FROMBT;
	
	return true;
}

void WebSerialSim::parsingCmd() {
	
	command = nullptr;
	argument = nullptr;
	
	// 1. comando principale
	command = strtok(buffer_ser, " ");
	if (command == NULL) {
		println("Comando Empty..");
		return;
	}
	
	// 2. primo parametro (se esiste)
	argument = strtok(NULL, " ");
	
	if (argument == nullptr) argument = command;
	
	// TIMESTAMP
	if (strcmp(command, "TIMESTAMP") == 0) {
		if (strcmp(argument, "ON") == 0) { 
			enableTimestamp = true;
			if (clientSSEGlobale) eventsserial->send("1", "timestamp_enabled", millis(), 0);
			printWeb("Timestamp abilitato\n");
		}
		else if (strcmp(argument, "OFF") == 0) { 
			enableTimestamp = false;
			if (clientSSEGlobale) eventsserial->send("0", "timestamp_enabled", millis(), 0);
			printWeb("Timestamp disabilitato\n");
		}
		//else { printWeb("Uso: TIMESTAMP ON|OFF\n"); }
		statoTask = IDLE;
		return;
	}
	
	// HISTORY
	if (strcmp(command, "HISTORY") == 0) {
		parsinghistory(argument);
		statoTask = IDLE;
		return;
	}
	
	//  CONFIG
	if (strcmp(command, "CONFIG") == 0) {
		if(stateRun){
			printWeb("Stoppare History prima!\n");
			} else if (strcmp(argument, "NOFS") == 0) {
			setHistoryFile(false);
			printWeb("Disabilitato LOG FS\n");
			} else if (strcmp(argument, "FS") == 0) {
			setHistoryFile(true);
			printWeb("Abilitato LOG FS\n");
			} else if (strcmp(argument, "NOPSRAM") == 0) {
			setPSRAM(false);
			} else if (strcmp(argument, "PSRAM") == 0) {
			setPSRAM(true);
			} else {
			//
			uint32_t amoutbuf = strtoul(argument, NULL, 10);
			setbuffer(amoutbuf);
			printfWeb("Dimensione buffer: %lu\n", amoutbuf);
		}
		statoTask = IDLE;
		return;
	} 
	// ALTRO
	if(_callback != nullptr) {
		if(argument != command) *(argument - 1) = ' '; 
		_callback(buffer_ser);
	}
	
	statoTask = IDLE;
}

void WebSerialSim::echoOnOff(bool onoff){
	echon = onoff;
}

// ======================
// CALLBACK ESTERNA
// ======================
void WebSerialSim::setCallback(CallbackFunzione cb) {
	_callback = cb;
	Serial.println(F("Callback registrata"));
}

// ======================
// CALLBACK BLE o altro
// ======================
void WebSerialSim::setCallBLE(CallbackBLE cb) {
	_callBLE = cb;
	Serial.println(F("Callback BLE registrata"));
}

// ======================
// SERVER INTERNO
// ======================
//void WebSerialSim::begin(int port) {
void WebSerialSim::begin(AsyncWebServer* mainServer) {
	
	//modestory(true);
	
	server = mainServer;
	
	// Inizializziamo l'Event Source (SSE)
	eventsserial = new AsyncEventSource("/events/serial");
	
	// Pagina HTML
	server->on("/serial", HTTP_GET, [&](AsyncWebServerRequest* request) {
		request->send_P(200, "text/html", serial_html);
	});
	
	// =====  VIEW E DOWNLOAD
	// Rotta per VIEW/DOWN il buffer o il history
	server->on("/buffer", HTTP_GET, [this](AsyncWebServerRequest *request) {
		
		char action[16] = {0};
		
		if (request->hasArg("action")) {
			request->arg("action").toCharArray(action, sizeof(action) - 1);
			} else {
			strcpy(action, "view");   // default
		}
		
		
		bool isfilehistory = false;
		// in ogni caso faccio un "fHistoryFlush()" e unrollbuffer  se esiste buffer
		// poi se view visualizzo  file o down faccio il download
		
		fHistoryFlush();	
		
		#ifdef _TYPE_FS
			if (FS_STORY.exists(FILE_HISTORY)) {
				isfilehistory = true;
			}
		#endif
		
		
		if (!isfilehistory && !historySerBuf){
			request->send(404, "text/plain", "File e buffer history non trovato");
			return;
		}
		
		if (strcmp(action, "down") == 0){
			#ifdef _TYPE_FS
				if(isfilehistory)	{
					AsyncWebServerResponse *response =
					request->beginResponse(FS_STORY, FILE_HISTORY, "text/plain");
					response->addHeader("Content-Disposition", "attachment; filename=history.txt");
					request->send(response);
				}
			#endif
			return;
		}
		
		if (strcmp(action, "view") == 0){
			if(isfilehistory){
				#ifdef _TYPE_FS
					request->send(FS_STORY, "/history.txt", "text/plain");
				#endif
				} else {
				if(pSerBuf > 0 || fullbuffer) request->send(200, "text/plain", historySerBuf);
			}
		}		
		// END
	});
	
	
	// rotta DELETE HISTORY
	server->on("/delhistory", HTTP_GET, [&](AsyncWebServerRequest* request) {
		if (!FS_STORY.exists(FILE_HISTORY)) {
			//request->send(404, "text/plain", "File history non trovato");
			request->send_P(200, "text/plain", "File history non trovato");
			return;
			}else {
			FS_STORY.remove(FILE_HISTORY);
			request->send_P(200, "text/html", "Deleted..");
		}
	});
	
	
	// POST parsingCmd
	server->on("/parsingCmd", HTTP_POST,
		[](AsyncWebServerRequest* request) {},
		nullptr,
		[&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
			
			if (len > LEN_BUF_SER - 1) {
				request->send(400, "text/plain", "Payload too large");
				return;
			}
			
			
			memcpy(buffer_ser + index, data, len);
			
			if (index + len == total) {
				buffer_ser[total] = '\0';
				fromin = FROMWEB;
				statoTask = PARSING;
				//request->send(200, "text/plain", "OK");
			}
			
			request->send(200, "text/plain", "OK");
		}
	);
	
	// SSE connect
	eventsserial->onConnect([&](AsyncEventSourceClient* client) {
		
		Serial.println(F("--- [SSE] Un browser si è appena connesso! ---"));
		
		//se esisteva un altro client connesso
		if(clientSSEGlobale){
			clientSSEGlobale->send("[SSE] Client Disconesso\n", "serial_print", millis(), 0);
		}
		
		clientSSEGlobale = client;
		// size_t totali = eventsserial.count() + 1;
		Serial.printf("[SSE] Client connessi totale: %d\n", eventsserial->count() + 1);
		
		if (statoTask == 0) {
			fromin = FROMWEB;
		}
		
	});
	
	// SSE disconnect
	eventsserial->onDisconnect([&](AsyncEventSourceClient* client) {
		
		// Questo metodo riceve nativamente il puntatore corretto quando un client si stacca
		
		Serial.printf("[SSE] Client disconnesso. Rimasti: %d\n", eventsserial->count());
		
		Serial.print("[SSE] Client disconnesso. \n");
		
		if (clientSSEGlobale == client) {
			clientSSEGlobale = nullptr;
		}
		
	});
	
	server->addHandler(eventsserial);
	
	Serial.println(F("WebSerialSim avviato"));
	modestory(true);
}
//===========================================================
// GESTIONE HISTORY su SRAM PSRAM su SD:
// HISTORY ON allocazione buffer SRAM o PSRAM e attivazione historySerBuf
// HISTORY OFF ferma registrazione e dealloca buffer
// HISTORY CLEAR cancella contenuto buffer della RAM
// HISTORY FLUSH scarica contenuto ram buffer su SD
// HISTORY VIEW visualizza contenuto ram buffer o file history se scrittura diretta
// HISTORY INFO statistica spazio buffer e spazio occupato
//====================================

void WebSerialSim::reverse(char* buf, size_t start, size_t end) {
	while (start < end) {
		char tmp = buf[start];
		buf[start] = buf[end];
		buf[end] = tmp;
		start++;
		end--;
	}
}

void WebSerialSim::unrollBuffer() {
	
	if (!fullbuffer || !historySerBuf || pSerBuf == 0) return;
	
	size_t head = pSerBuf;
	size_t tail = dimSerBuf - pSerBuf;
	
	// 1. Reverse HEAD
	if (head > 0)
	reverse(historySerBuf, 0, head - 1);
	
	// 2. Reverse TAIL
	if (tail > 0)
	reverse(historySerBuf, head, dimSerBuf - 1);
	
	// 3. Reverse tutto
	reverse(historySerBuf, 0, dimSerBuf - 1);
	
	pSerBuf = 0;
	tailBuf = 0;
}


void WebSerialSim::modestory(bool action) {
	
	if (action && !stateRun) {
		makeBuffer();
		(dimSerBuf == 0 && historyFileEnabled) ? directFS = true: directFS = false;
		stateRun = true;	// in ogni caso
		infoSerBuf();
		return;
		}else if (!action && stateRun) {
		// Pausa HISTORY
		stateRun = false;
		//if (historySerBuf) {
		fHistoryFlush();     // salva su file prima di liberare
		//}
		infoSerBuf();
	}
}

void WebSerialSim::setHistoryFile(bool enable) {
	#ifdef _TYPE_FS
	historyFileEnabled = enable;
	#endif
}

void WebSerialSim::setbuffer(size_t _dimbuffer) {
	tmpdimSerBuf = _dimbuffer;
	if(_dimbuffer != 0 && _dimbuffer < 1500)
	Serial.println(F("Attenzione buffer too small ..almeno 1500"));
}

void WebSerialSim::setPSRAM(bool _enable) {
	inPSRAM = _enable;
}

void WebSerialSim::parsinghistory(char *opzion) {
	//Serial.println(opzion);
	
	if (strstr(opzion, "ON") != NULL) {
		modestory(true);
		//printWeb("-HISTORY ATTIVO\n");
		} else if (strstr(opzion, "OFF") != NULL) {
		modestory(false);
		} else if (strstr(opzion, "CLEAR") != NULL) {
		memset(historySerBuf, 0, dimSerBuf + 1);
		pSerBuf = 0;
		tailBuf = 0;
		fullbuffer = false;
		//	} else if (strstr(opzion, "FLUSH") != NULL) {
		//fHistoryLoad();
		} else if (strstr(opzion, "LOAD") != NULL) {
		fHistoryLoad();
		} else if (strstr(opzion, "INFO") != NULL) {
		infoSerBuf();
		} else {
		printWeb("-OPZIONE non valida!\n");
	}
	
}


bool WebSerialSim::makeBuffer() {
	
	if(dimSerBuf == tmpdimSerBuf) return true;
	
	if(historySerBuf) {
		free(historySerBuf);
		historySerBuf = nullptr;
	}
	
	dimSerBuf = tmpdimSerBuf;
	pSerBuf = 0;
	tailBuf = 0;
	fullbuffer = false;
	
	if (dimSerBuf == 0) return true;
		
	// ============================
	// 1. Tentativo PSRAM
	// ============================
	if (inPSRAM && psramFound()) {
		
		Serial.printf("Richiedo n:%lu byte\n", dimSerBuf);
		
		historySerBuf = (char*) heap_caps_malloc(
			dimSerBuf + 1,
			MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
		);
		
		if (historySerBuf) {
			Serial.printf("-Allocato %u byte in PSRAM\n", dimSerBuf);
		}
	}
	
	// ============================
	// 2. Fallback su SRAM interna
	// ============================
	if (!historySerBuf) {
		
		Serial.println(F("-Allocazione PSRAM fallita, uso SRAM"));
		
		historySerBuf = (char*) heap_caps_malloc(
			dimSerBuf + 1,
			MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
		);
		
		if (historySerBuf) {
			Serial.printf("-Allocato %u byte in SRAM\n", dimSerBuf);
		}
	}
	
	// ============================
	// 3. Fallimento totale
	// ============================
	if (!historySerBuf) {
		printWeb("-ERRORE allocazione Buffer HISTORY\n");
		stateRun = false;
		dimSerBuf = 0;
		return false;
	}
	
	memset(historySerBuf, 0, dimSerBuf + 1);
	printWeb("-HISTORY buffer creato\n");
	stateRun = true;
	
	return true;
}

void WebSerialSim::fHistoryFlush() {
	
	bool riaccendi = stateRun;
	stateRun = false;
	
	if(historyFileEnabled && !directFS && pSerBuf > 0) {
		#ifdef _TYPE_FS
			fregbuffer();
		#endif	
	}
	
	unrollBuffer();
		
	stateRun = riaccendi;
}

void WebSerialSim::fHistoryLoad() {
	
	if(!historySerBuf) return;
	if (pSerBuf == 0 && !fullbuffer) return;
	
	bool riaccendi = stateRun;
	stateRun = false;
	bool stamptime = enableTimestamp;
	enableTimestamp = false;
	
	//printWeb("=== Load buffer ===\n");
	if(fullbuffer && pSerBuf != 0) unrollBuffer();
	if(fullbuffer){
		printBigBuf(historySerBuf, dimSerBuf);
		} else {
		printBigBuf(historySerBuf, pSerBuf);
	}
	//printWeb("=== End Load ===\n");
	
	stateRun = riaccendi;
	enableTimestamp = stamptime;
	return;
}

void WebSerialSim::insHistory(const char* str){
	
	if (!stateRun) return;
	
	#ifdef _TYPE_FS
		if(directFS && stateRun && historyFileEnabled){
			File hfile = FS_STORY.open(FILE_HISTORY, FILE_APPEND);
			if (enableTimestamp) hfile.write((const uint8_t*)_timestamp, 11);
			hfile.write((const uint8_t*)str, strlen(str));
			hfile.close();
			return;
		}
	#endif
	
	int dimstr = strlen(str);
	// per sicurezza se la str e' piu lunga del buffer ..tronco
	// accertarsi sempre che il buffer sia almeno 1500 byte
	if(dimstr >= dimSerBuf) dimstr = dimSerBuf -1;
	
	if (!historySerBuf) return;
	
	int spazio_alla_fine = dimSerBuf - pSerBuf;
	
	if (dimstr <= spazio_alla_fine) {
		// Caso 1: La stringa ci sta tutta di seguito fino alla fine del buffer
		memcpy(&historySerBuf[pSerBuf], str, dimstr);
		pSerBuf += dimstr;
		if (pSerBuf == dimSerBuf){
			#ifdef _TYPE_FS
				fregbuffer();
			#endif
			pSerBuf = 0;
			fullbuffer = true;
			// rec su sd prima di sovrascrivere ------------------
			
		}
		return;
	}
	// Caso 2: La stringa si deve spezzare in due (una parte alla fine, il resto all'inizio)
	int prima_parte = spazio_alla_fine;
	int seconda_parte = dimstr - spazio_alla_fine;
	
	memcpy(&historySerBuf[pSerBuf], str, prima_parte);
	pSerBuf = dimSerBuf;
	
	// rec su sd prima di sovrascrivere ------------------
	#ifdef _TYPE_FS
		fregbuffer();
	#endif
	
	memcpy(historySerBuf, &str[prima_parte], seconda_parte);
	
	pSerBuf = seconda_parte;
	fullbuffer = true; // Il buffer ha completato almeno un giro completo
	
}

void WebSerialSim::infoSerBuf() {
	
	bool riaccendi = stateRun;
	stateRun = false;
	
	#ifdef _TYPE_FS
		printfWeb("-LOG File: %s\n", historyFileEnabled ? "ATTIVO" : "NON attivo");
	#endif
	
	if (!historySerBuf) {
		#ifdef _TYPE_FS
			if (directFS && historyFileEnabled)
			printWeb("-LOG diretto su file\n");
		#endif
		printWeb("-BUFFER non presente\n");
		} else {
		printfWeb("-%s Buffer %u byte, %u Occupati\n", inPSRAM ? "PSRAM" : "SRAM",
		dimSerBuf, fullbuffer ? dimSerBuf : pSerBuf);
	}
	
	printfWeb("-STATO %s\n", riaccendi ? "RUN" : "PAUSA");
	
	stateRun = riaccendi;
}


void WebSerialSim::fregbuffer() {
	#define oldstory "/oldstory.txt"
	
	if (!historyFileEnabled) return;
	
	if (pSerBuf == tailBuf && !fullbuffer) {
    return;
	}
	
	#ifdef _TYPE_FS
		
		File h = FS_STORY.open(FILE_HISTORY, FILE_APPEND);
		if(!h) return;
		bool riaccendi = stateRun;
		stateRun = false;
				
		// Se tailBuf è dietro a pSerBuf, i dati sono in un unico blocco continuo
		if (pSerBuf > 0) {
			h.write((const uint8_t*)&historySerBuf[tailBuf], pSerBuf - tailBuf);
		} 
		
		tailBuf = pSerBuf;
		if(tailBuf == dimSerBuf) tailBuf = 0;
		//file.flush(); // Assicura la scrittura fisica su SD
		
		stateRun = riaccendi;
		
		if (h.size() > MAXSIZEFILE_HISTORY) {
			h.close();
			FS_STORY.remove(oldstory);
			FS_STORY.rename(FILE_HISTORY, oldstory);
			} else {
			h.close();
		}
	#endif
}

WebSerialSim::~WebSerialSim() {
	if (eventsserial) {
		delete eventsserial;
		eventsserial = nullptr;
	}
	if (historySerBuf) {
		free(historySerBuf);
		historySerBuf = nullptr;
	}
	/* if (clientsMutex) {
		vSemaphoreDelete(clientsMutex);
		clientsMutex = nullptr;
	} */
}					
