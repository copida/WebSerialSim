#include "WebSerialSim.h"
#include <stdarg.h>
#include <time.h>

// ======================
// COSTRUTTORE
// ======================
WebSerialSim::WebSerialSim() {
	
	server = nullptr;
	eventsserial = nullptr;
	
	_mutex = xSemaphoreCreateMutex();
	
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
	
	#ifdef PSRAM
		inPSRAM = true;
		#else
		inPSRAM = false;
	#endif
	
	bufIndexIn = 0;
	
	command = nullptr;
	argument = nullptr;
	
	echon = false;
	
	fromin = FROMWEB;
	statoTask = IDLE;
	_nerrTX = 0;
	
	_callback = nullptr;
	_callBLE = nullptr;
	
	//targetClient = nullptr;
	clientSSEGlobale = nullptr;
	rawClient = nullptr;
	//max_mtu = CHUNK_SIZE + 64;
	
}

void WebSerialSim::handleBufferIn() {
	if (bufIndexIn == 0) return;
	
	if (millis() - ultimoCarattereTime >= TIMEOUT_MS) {
		bufferIn[bufIndexIn] = '\0';
		printWeb(bufferIn);
		bufIndexIn = 0;
		bufferIn[0] = 0;
	}
}

/* void WebSerialSim::printWeb(char* _datiprint, size_t quantsize) {
	if (quantsize == 0) quantsize = strlen(_datiprint);
	if (quantsize > CHUNK_SIZE) {
	printBigBuf(_datiprint, quantsize);
	} else {
	sendWeb(_datiprint, quantsize);
	}
} */

void WebSerialSim::sendWeb(char* _dati, size_t len) {
	
	if (enableTimestamp) getTimestampString();
	
	//	if (fromin == FROMWEB && clientSSEGlobale) {
	if (clientSSEGlobale) {
		if (enableTimestamp) {			
			txSSE(_timestamp, 11, true);
		}
		//txSSE((const char*)_dati, len);
		if(!txSSE(_dati, len, false)) Serial.write(_dati, len);
	}
	
	// 1. Echo su Serial (se abilitato)
	if (fromin == FROMSER || echon) {
		if (enableTimestamp) Serial.print(_timestamp);
		Serial.write(_dati, len);
	}
	
	
	#ifdef OUTBLE
		if (fromin == FROMBT) if(_callBLE)_callBLE(_dati);
	#endif
	
	//  Accumulo nel buffer di cronologia (PSRAM o SRAM)
	if (stateRun) {
		if (enableTimestamp) {
			insBuffer(_timestamp);
		}
		insBuffer(_dati);
	}
	
}

// ===== TIMESTAMP HELPER =====
void WebSerialSim::getTimestampString() {
	//static char timestamp[16];  // "[HH:MM:SS] " = 11 char max
	
	
	if (!enableTimestamp) {
		return;
	}
	
	#ifdef TIMESTAMP_REALTIME
		time_t now = time(nullptr);
		struct tm* timeinfo = localtime(&now);
		if (timeinfo == nullptr) return;
		
		snprintf(_timestamp, sizeof(_timestamp),
			"[%02d:%02d:%02d] ",
			timeinfo->tm_hour,
			timeinfo->tm_min,
		timeinfo->tm_sec);
		
		#else
		
    unsigned long ms = millis();
    
    unsigned long totalSecs = ms / 1000;
    int hours = (totalSecs / 3600) % 24;
    int minutes = (totalSecs / 60) % 60;
    int seconds = totalSecs % 60;
    
    snprintf(_timestamp, sizeof(_timestamp),
			"[%02d:%02d:%02d] ",
		hours, minutes, seconds);
		
	#endif
	
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
	if (size + bufIndexIn >= DIMBUFFERIN - 1) {
		if(bufIndexIn > 0){
		printWeb(bufferIn, bufIndexIn);
		bufIndexIn = 0;
	}
}

// Se il blocco singolo è più grande dell'intero buffer vuoto, bypassa l'accumulo
if (size >= DIMBUFFERIN - 1) {
	printWeb((char*)buffer, size);
	return size;
}

memcpy(&bufferIn[bufIndexIn], buffer, size);
bufIndexIn += size;
bufferIn[bufIndexIn] = '\0';

if (bufferIn[bufIndexIn - 1] == '\n') {
	//bufIndexIn = 0;
	printWeb(bufferIn, bufIndexIn);
	bufIndexIn = 0;
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

//  per gestire la macro F()
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
void WebSerialSim::printWeb(char *bigbuf, size_t dim) {
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
		sendWeb(chunkBuf, sendLen);
		srcIndex += sendLen;
	}
	// end while
}

bool WebSerialSim::txSSE(char* _datiprint, size_t requiredSpace, bool txtimestamp) {
	
	AutoLock lock(_mutex, pdMS_TO_TICKS(100));
	if (!lock.isLocked()) return false;
	
	#define timeoutbuffer 2000
	#define TICKSBASE 2
	#define MAX_TENTATIVI 50
	
	int tentativi = 0;
	bool invioRiuscito = false;
	uint32_t startWait = millis();
	
  while (tentativi < MAX_TENTATIVI) {
		
		// Verifica stabilità connessione nel ciclo
    if (clientSSEGlobale == nullptr || !clientSSEGlobale->connected()  || rawClient == nullptr) {
			Serial.println(F("Client NO"));
      break;
		}
		
		if(rawClient->space() > requiredSpace && rawClient->canSend()){
			if(txtimestamp) {
				invioRiuscito = eventsserial->send((const char*)_timestamp, "timestamp", startWait, 0);
				} else {
				invioRiuscito = eventsserial->send((const char*)_datiprint, "serial_print", startWait, 0);
			}
		}
		
		if (invioRiuscito) {
			break;
		}
		
		// TCP pronto ..coda SSE non pronta (max 32 messaggi)
		tentativi++;	
		yield();
		vTaskDelay(pdMS_TO_TICKS(tentativi + TICKSBASE));
		
		if (millis() - startWait > timeoutbuffer){
			Serial.println(F("TIMEOUT"));
			break;
		}
		
	}	// end while
	
	//if(tentativi > 0) Serial.println(tentativi);
	if(!invioRiuscito) {
		_nerrTX++;
		//Serial.write(_datiprint, requiredSpace);
		return false;
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
	if (lenb >= LEN_BUF_SER)
	lenb = LEN_BUF_SER - 1;
	
	memcpy(buffer_ser, inExt, lenb);
	buffer_ser[lenb] = '\0';
	
	statoTask = PARSING;
	fromin = FROMBT;
	
	return true;
}

void WebSerialSim::parsingCmd() {
	
	/* command = nullptr;
	argument = nullptr;
	
	// comando principale
	char* firstSpace = strchr(localBuf, ' ');
	command = strtok(buffer_ser, " ");
	
	if (command == NULL) {
		println(F("Comando Empty.."));
		return;
	}
	//Serial.println(command);
	// primo parametro (se esiste)
	argument = strtok(NULL, " ");
	
	if (argument == nullptr) argument = command; */
	
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
			printWeb("Comando vuoto\n");
        statoTask = IDLE;
        return;
    }
	
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
		//if(argument != command) *(argument - 1) = ' '; 
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
void WebSerialSim::begin(AsyncWebServer* mainServer) {
	
	server = mainServer;
	
	#ifndef INTERNALHTML
		//per permettere collegamento esterno
		DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
	#endif
	
	// Inizializziamo l'Event Source (SSE)
	eventsserial = new AsyncEventSource("/events/serial");
	
	server->addHandler(eventsserial);
	
	
	server->onNotFound([](AsyncWebServerRequest *request) {
		if (request->method() == HTTP_OPTIONS) {
			request->send(200);
			} else {
			request->send(404);
		}
	});
	
	#ifdef INTERNALHTML
		// Pagina HTML	
		#ifdef GZIP
			server->on("/serial", HTTP_GET, [](AsyncWebServerRequest *request){
				AsyncWebServerResponse *response = request->beginResponse_P(
					200, 
					"text/html", 
					(const uint8_t*) html_gz, 
					html_gz_len
				);
				response->addHeader("Content-Encoding", "gzip");
				request->send(response);
			});
			#else
			server->on("/serial", HTTP_GET, [&](AsyncWebServerRequest* request) {
				request->send_P(200, "text/html", serial_html);
			});
		#endif
	#endif
	
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
    
    // Sincronizza i buffer prima di leggere
    fHistoryFlush();    
    
    #ifdef _TYPE_FS
			if (FS_DRV.exists(FILE_HISTORY)) {
				isfilehistory = true;
			}
		#endif
    
    if (!isfilehistory && !historySerBuf) {
			request->send(404, "text/plain", "File e buffer history non trovato");
			return;
		}
    
    // ----------------------------------------------------
    // GESTIONE DOWNLOAD (action=down)
    // ----------------------------------------------------
    if (strcmp(action, "down") == 0) {
			#ifdef _TYPE_FS
				if (isfilehistory) {
					AsyncWebServerResponse *response = nullptr;
					
					#ifdef HISTORY_sdFAT
						// SdFat richiede l'apertura manuale del file per lo streaming
						FS_FILE_TYPE fileToDownload = FS_DRV.open(FILE_HISTORY, MOD_READ);
						if (fileToDownload) {
							response = request->beginResponse(fileToDownload, FILE_HISTORY, "text/plain");
							} else {
							request->send(404, "text/plain", "Errore apertura file log");
							return;
						}
						#else
						response = request->beginResponse(FS_DRV, FILE_HISTORY, "text/plain");
					#endif
					
					if (response != nullptr) {
						response->addHeader("Content-Disposition", "attachment; filename=history.txt");
						request->send(response);
					}
				}
			#endif
			return;
		}
    
    // ----------------------------------------------------
    // GESTIONE VISUALIZZAZIONE A SCHERMO (action=view)
    // ----------------------------------------------------
    if (strcmp(action, "view") == 0) {
			if (isfilehistory) {
				#ifdef _TYPE_FS
					#ifdef HISTORY_sdFAT
						FS_FILE_TYPE fileToView = FS_DRV.open(FILE_HISTORY, MOD_READ);
						if (fileToView) {
							request->send(fileToView, FILE_HISTORY, "text/plain");
							} else {
							request->send(404, "text/plain", "Errore apertura file log");
						}
						#else
						request->send(FS_DRV, FILE_HISTORY, "text/plain");
					#endif
				#endif
        } else {
				if (pSerBuf > 0 || fullbuffer) {
					request->send(200, "text/plain", historySerBuf);
				}
			}
			return;
		}       
	});
	
	
	// rotta DELETE HISTORY
	server->on("/delhistory", HTTP_GET, [&](AsyncWebServerRequest* request) {
		#ifdef _TYPE_FS
			if (!FS_DRV.exists(FILE_HISTORY)) {
				//request->send(404, "text/plain", "File history non trovato");
				request->send_P(200, "text/plain", "File history non trovato");
				return;
				}else {
				FS_DRV.remove(FILE_HISTORY);
				request->send_P(200, "text/html", "Deleted..");
			}
			#else
			request->send_P(200, "text/html", "OK");
		#endif
	});
	
	
	// POST parsingCmd
	server->on("/parsingCmd", HTTP_POST,
		[](AsyncWebServerRequest* request) {},
		nullptr,
		[&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
			
			// if (len > LEN_BUF_SER - 1) {
			// request->send(400, "text/plain", "Payload too large");
			// return;
			// }
			
			//if (index + len > LEN_BUF_SER - 1)
			if (total >= LEN_BUF_SER || index + len > LEN_BUF_SER - 1) {
				request->send(400, "text/plain", "Payload too large");
				return;
			}
			
			memcpy(buffer_ser + index, data, len);
			
			if (index + len == total) {
				buffer_ser[total] = '\0';
				fromin = FROMWEB;
				statoTask = PARSING;
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
		if (clientSSEGlobale != nullptr) {
			rawClient = clientSSEGlobale->client();
		}
		Serial.printf("[SSE] Client connessi totale: %d\n", eventsserial->count() + 1);
		if(enableTimestamp) clientSSEGlobale->send("1", "timestamp_enabled", millis(), 0);
		
		if (statoTask == 0) {
			fromin = FROMWEB;
		}
		
	});
	
	// SSE disconnect
	eventsserial->onDisconnect([&](AsyncEventSourceClient* client) {
		
		Serial.printf("[SSE] Client disconnesso. Rimasti: %d\n", eventsserial->count());
		
		//Serial.print("[SSE] Client disconnesso. \n");
		
		if (clientSSEGlobale == client) {
			clientSSEGlobale = nullptr;
		}
		
	});
	
	//eventsserial->addHeader("Access-Control-Allow-Origin", "*");
	//DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
	//server->addHandler(eventsserial);
	
	// !!! RIGA FONDAMENTALE PER IL CORS !!!
	// Permette a qualsiasi origine esterna (incluso il tuo PC) di connettersi a questa route SSE
	//eventsserial->addHeader("Access-Control-Allow-Origin", "*");
	
	Serial.println(F("WebSerialSim avviato"));
	playstory(true);
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
	
	AutoLock lock(_mutex);
	if (!lock.isLocked()) return;
	
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


void WebSerialSim::playstory(bool action) {
	
	if (action && !stateRun) {
		makeBuffer();
		(dimSerBuf == 0 && historyFileEnabled) ? directFS = true: directFS = false;
		stateRun = true;	// in ogni caso
		infoSerBuf();
		return;
		}else if (!action && stateRun) {
		// Pausa HISTORY
		stateRun = false;
		fHistoryFlush();     // salva su file prima di liberare
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

void WebSerialSim::setTimestamp(bool _enable) {
	enableTimestamp = _enable;
}


void WebSerialSim::parsinghistory(char *opzion) {
	//Serial.println(opzion);
	
	if (strstr(opzion, "ON") != NULL) {
		playstory(true);
		//printWeb("-HISTORY ATTIVO\n");
		} else if (strstr(opzion, "OFF") != NULL) {
		playstory(false);
		} else if (strstr(opzion, "CLEAR") != NULL) {
		if (historySerBuf) {
			memset(historySerBuf, 0, dimSerBuf + 1);
			pSerBuf = 0;
			tailBuf = 0;
			fullbuffer = false;
		}
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
	#ifdef PSRAM
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
	#endif
	
	// ============================
	// 2. Fallback su SRAM interna
	// ============================
	if (!historySerBuf) {
		#ifdef PSRAM
			Serial.println(F("-Allocazione PSRAM fallita, uso SRAM"));
		#endif
		
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
		printWeb(historySerBuf, dimSerBuf);
		} else {
		printWeb(historySerBuf, pSerBuf);
	}
	//printWeb("=== End Load ===\n");
	
	stateRun = riaccendi;
	enableTimestamp = stamptime;
	return;
}

void WebSerialSim::insBuffer(const char* str){
	
	if (!stateRun) return;
	
	
	AutoLock lock(_mutex);
	if (!lock.isLocked()) return;
	
	#ifdef _TYPE_FS
		if(directFS && stateRun && historyFileEnabled){
			FS_FILE_TYPE hfile = FS_DRV.open(FILE_HISTORY, MOD_APPEND);
			if (enableTimestamp) hfile.write((const uint8_t*)_timestamp, 11);
			hfile.write((const uint8_t*)str, strlen(str));
			hfile.close();
			return;
		}
	#endif
	
	if (!historySerBuf || dimSerBuf == 0) {
    return;  // Buffer non inizializzato
	}
	
	int dimstr = strlen(str);
	// per sicurezza se la str e' piu lunga del buffer ..tronco
	if(dimstr >= dimSerBuf) dimstr = dimSerBuf -1;
	
	
	int spazio_alla_fine = dimSerBuf - pSerBuf;
	
	if (dimstr <= spazio_alla_fine) {
		//La stringa ci sta tutta di seguito fino alla fine del buffer
		memcpy(&historySerBuf[pSerBuf], str, dimstr);
		pSerBuf += dimstr;
		if (pSerBuf == dimSerBuf){
			// rec su sd prima di sovrascrivere ------------------
			#ifdef _TYPE_FS
				fregbuffer();
			#endif
			pSerBuf = 0;
			fullbuffer = true;
		}
		return;
	}
	// La stringa si deve spezzare in due (una parte alla fine, il resto all'inizio)
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
	fullbuffer = true; 
	
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
	
	if(_nerrTX >0) printfWeb("-Err. TX %d\n", _nerrTX);
	//printfWeb("-MAX MTU %d\n", max_mtu);
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
		
		FS_FILE_TYPE h = FS_DRV.open(FILE_HISTORY, MOD_APPEND);
		if(!h) return;
		bool riaccendi = stateRun;
		stateRun = false;
		
		if (pSerBuf > 0) {
			h.write((const uint8_t*)&historySerBuf[tailBuf], pSerBuf - tailBuf);
		} 
		
		tailBuf = pSerBuf;
		if(tailBuf == dimSerBuf) tailBuf = 0;
		
		stateRun = riaccendi;
		
		if (h.size() > MAXSIZEFILE_HISTORY) {
			h.close();
			FS_DRV.remove(oldstory);
			FS_DRV.rename(FILE_HISTORY, oldstory);
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
	clientSSEGlobale = nullptr;
	
	if (_mutex != nullptr) {
		vSemaphoreDelete(_mutex);
		_mutex = nullptr;
	}
	/* if (clientsMutex) {
		vSemaphoreDelete(clientsMutex);
		clientsMutex = nullptr;
	} */
}					
