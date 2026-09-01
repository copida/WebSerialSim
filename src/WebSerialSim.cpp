#include "WebSerialSim.h"
#include <stdarg.h>

// ======================
// COSTRUTTORE
// ======================
WebSerialSim::WebSerialSim() {
	
	server = nullptr;
	eventsserial = nullptr;
	
	historySerBuf = nullptr;
	actSerBuf = false;
	pSerBuf = 0;
	dimSerBuf = MAXSIZEBUFFER_HISTORY;
	fullbuffer = false;
	
	//inPSRAM = false;
	#ifdef BUFFER_PSRAM
		bool inPSRAM = true;
		#else
		bool inPSRAM = false;
	#endif
	
	
	bufIndexIn = 0;		//
	
	comando = nullptr;
	param1 = nullptr;
	
	echon = false;
	
	fromin = FROMSER;
	statoTask = IDLE;
	
	//pwd_executive = (char*)"Admin57";
	
	_callback = nullptr;
	_callBLE = nullptr;
	targetClient = nullptr;
}

// Il cuore del sistema non bloccante
void WebSerialSim::handleBufferIn() {
	if (bufIndexIn == 0) return;
	
	if (millis() - ultimoCarattereTime >= TIMEOUT_MS) {
		bufferIn[bufIndexIn] = '\0'; // Sicurezza: garantisci il terminatore prima di stampare
		printWeb(bufferIn);
		bufIndexIn = 0;
		bufferIn[0] = 0;
	}
}

void WebSerialSim::printWeb(char* _datiprint) {
	// 1430 byte è perfetto per lasciare spazio ai metadata SSE nel frame TCP
	if (strlen(_datiprint) > 1430) {
		printBigBuf(_datiprint, strlen(_datiprint));
    } else {
		sendWeb(_datiprint, strlen(_datiprint));
	}
}


void WebSerialSim ::sendWeb(char* _dati, size_t len) {
	
	// 1. Echo su Serial (se abilitato)
	if (fromin == FROMSER || echon) {
		Serial.write(_dati, len);
	}
	
	if (fromin == FROMWEB && targetClient) {
		if (canSendSSE(len)) {
			targetClient->send((const char*)_dati, "serial_print", millis(), 0);
			} else {
			// Fallback su Serial se SSE non può inviare
			Serial.write(_dati, len);
		}
	}
	
	#ifdef OUTBLE
		if (fromin == FROMBT) if(_callBLE)_callBLE(_dati);
	#endif
	
	// 3. Accumulo diretto nel buffer di cronologia (PSRAM o SRAM)
	if (actSerBuf) {
		insHistory(_dati);
	}
	
}

//======================
// size_t WebSerialSim::write(uint8_t c):
// size_t WebSerialSim::write(const uint8_t *buffer, size_t size):
//======================
size_t WebSerialSim::write(uint8_t m) {
	// Chiamata diretta e sicura alla logica interna
	return write(&m, 1);
}

size_t WebSerialSim::write(const uint8_t *buffer, size_t size) {
	if (size == 0 || buffer == nullptr) return 0;
	
	// Se il blocco in arrivo non sta nel buffer rimasto, svuota prima il buffer attuale
	if (size + bufIndexIn >= DIMBUFFERIN - 1 && bufIndexIn != 0) {
		bufferIn[bufIndexIn] = '\0';
		printWeb(bufferIn);
		bufIndexIn = 0;
	}
	
	// Se il blocco singolo è più grande dell'intero buffer vuoto, bypassa l'accumulo
	if (size >= DIMBUFFERIN - 1) {
		printBigBuf((char*)buffer, size);
		return size;
	}
	
	// Ora la copia è sicura al 100% da buffer overflow
	memcpy(&bufferIn[bufIndexIn], buffer, size);
	bufIndexIn += size;
	bufferIn[bufIndexIn] = '\0'; // Terminatore sicuro
	
	if (bufferIn[bufIndexIn - 1] == '\n') {
		printWeb(bufferIn);
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


void WebSerialSim::switchState(int fasestate){
	statoTask = fasestate;
}


void WebSerialSim::printBigBuf(char *bigbuf, size_t dim) {
	if (dim == -1 || dim == 0) dim = strlen(bigbuf);
	
	const size_t CHUNK_SIZE = 1440; // Ottimizzato sotto la soglia MTU standard (1460)
	char chunkBuf[CHUNK_SIZE];
	char lineBuf[512];
	
	chunkBuf[0] = '\0';
	size_t chunkLen = 0;
	size_t lineLen = 0;
	size_t ibigbuf = 0;
	
	while (ibigbuf < dim) {
		char c = bigbuf[ibigbuf++];
		
		if (c == '\n') {
			// Se la stringa è solo un ritorno a capo o è vuota
			//if(lineLen == 0){
			//	lineBuf[lineLen++] = ' ';
			//}
			lineBuf[lineLen++] = '\n';
			lineBuf[lineLen] = '\0';
			
			// Se aggiungere questa riga supera il chunk, svuota il chunk in rete
			if (chunkLen + lineLen >= CHUNK_SIZE - 1) {
				sendWeb(chunkBuf, chunkLen);
				chunkBuf[0] = '\0';
				chunkLen = 0;
				delay(2); // Cede il passo allo stack di rete AsyncTCP
			}
			
			memcpy(chunkBuf + chunkLen, lineBuf, lineLen);
			chunkLen += lineLen;
			chunkBuf[chunkLen] = '\0';
			lineLen = 0;
			} else {
			// Se la riga supera la dimensione massima di sicurezza, forza uno split artificiale
			if (lineLen >= sizeof(lineBuf) - 2) {
				lineBuf[lineLen] = '\0';
				if (chunkLen + lineLen >= CHUNK_SIZE - 1) {
					sendWeb(chunkBuf, chunkLen);
					chunkBuf[0] = '\0';
					chunkLen = 0;
					delay(2);
				}
				memcpy(chunkBuf + chunkLen, lineBuf, lineLen);
				chunkLen += lineLen;
				chunkBuf[chunkLen] = '\0';
				lineLen = 0;
			}
			lineBuf[lineLen++] = c;
		}
	}
	
	// Gestione residuo ultima riga
	if (lineLen > 0) {
		lineBuf[lineLen] = '\0';
		if (chunkLen + lineLen >= CHUNK_SIZE - 1) {
			sendWeb(chunkBuf, chunkLen);
			chunkBuf[0] = '\0';
			chunkLen = 0;
		}
		memcpy(chunkBuf + chunkLen, lineBuf, lineLen);
		chunkLen += lineLen;
	}
	
	// Invio finale del chunk residuo
	if (chunkLen > 0) {
		// Se non finisce con \n, lo aggiungiamo in modo sicuro senza sforare
		if (chunkBuf[chunkLen - 1] != '\n' && chunkLen < CHUNK_SIZE - 1) {
			chunkBuf[chunkLen++] = '\n';
		}
		chunkBuf[chunkLen] = '\0';
		sendWeb(chunkBuf, chunkLen);
	}
}

// ======================
// SSE
// ======================
bool WebSerialSim::checkClientSSE() {
	return (eventsserial->count() > 0);
}

bool WebSerialSim::canSendSSE(size_t requiredSpace) {
	
	if (eventsserial->count() == 0)
	return false;
	
	uint32_t startWait = millis();
	
	while (targetClient &&
		(targetClient->client()->space() < (requiredSpace + 64) ||
		!targetClient->client()->canSend())) {
		
		if (eventsserial->count() == 0 || !targetClient->connected()) {
			targetClient = nullptr;
			Serial.println(F("Client disconnesso!"));
			return false;
		}
		
		if (millis() - startWait > 2000) {
			Serial.println(F("[SSE] Timeout buffer"));
			return false;
		}
		
		delay(1);
		yield();
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
	char ch_tmp;
	bool end_input = false;
	
	while (Serial.available()) {
		
		ch_tmp = Serial.read();
		
		if (ch_tmp == '\n') {
			buffer_ch[p_buf] = '\0';
			end_input = true;
			} else if (ch_tmp != '\r') {
			buffer_ch[p_buf] = ch_tmp;
			if (p_buf < max_ch) p_buf++;
		}
		
		delay(5);
	}
	
	if (end_input) {
		p_buf = 0;
		//trimmer(buffer_ch);
		
		if (statoTask != IDLE) {
			Serial.println(F("Sistema occupato"));
			} else {
			fromin = FROMSER;
			statoTask = PARSING;
		}
	}
}

bool WebSerialSim::inputEXT(char* inExt, int lenb) {
	
	if (statoTask != IDLE)
	return false;
	
	memcpy(buffer_ser, inExt, lenb);
	statoTask = PARSING;
	fromin = FROMBT;
	
	return true;
}

void WebSerialSim::parsingCmd() {
	
	comando = buffer_ser;
	//param1 = nullptr;
	
	if (strstr(buffer_ser, "HISTORY") != NULL){
		parsinghistory(comando);
		}else{
		if(_callback != nullptr){
			_callback(comando, param1);
		}
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
// UTILITY
// ======================
// void WebSerialSim::appbuf(const char* str, char* destBuf, size_t maxDim, size_t* pIndice) {

// while (*str && *pIndice < maxDim - 2) {
// destBuf[*pIndice] = *str;
// (*pIndice)++;
// str++;
// }

// if (destBuf[*pIndice - 1] != '\n') {
// destBuf[*pIndice] = '\n';
// (*pIndice)++;
// }

// destBuf[*pIndice] = '\0';
// }

// ======================
// SERVER INTERNO
// ======================
//void WebSerialSim::begin(int port) {
void WebSerialSim::begin(AsyncWebServer* mainServer) {
	
	
	//server = new AsyncWebServer(port);
	// Salviamo il riferimento al server dello sketch
	server = mainServer;
	
	// Inizializziamo l'Event Source (SSE)
	eventsserial = new AsyncEventSource("/events/serial");
	
	clientsMutex = xSemaphoreCreateMutex();
	
	// Pagina HTML
	server->on("/serial", HTTP_GET, [&](AsyncWebServerRequest* request) {
		request->send_P(200, "text/html", serial_html);
	});
	
	// Rotta per servire il buffer di testo
  server->on("/view-buffer", HTTP_GET, [this](AsyncWebServerRequest *request){
    // Invia il buffer direttamente dalla memoria
		if(directFS){
			#ifdef FS_STORY
			if (FS_STORY.exists("/history.txt")) {
				// Invia il file:
				// Parametri: LittleFS, percorso file, mime-type (text/plain)
				request->send(FS_STORY, "/history.txt", "text/plain");
				} else {
				request->send(404, "text/plain", "File non trovato!");
			}
			#endif
			}else {
			unrollBuffer();
			request->send(200, "text/plain", historySerBuf);
		}
		
	});
	
	// CONTEGGIO clients ONLINE
	
	server->on("/get-clientcount", HTTP_GET, [this](AsyncWebServerRequest* request) {
		char countBuf[8];
		snprintf(countBuf, sizeof(countBuf), "%u", (unsigned int)activeClients.size());
		if (eventsserial != nullptr) { // o eventsserial
			eventsserial->send(countBuf, "client_count", millis(), 0);
		}
		request->send(200, "text/plain", countBuf);
	});
	
	// POST parsingCmd
	server->on("/parsingCmd", HTTP_POST,
		[](AsyncWebServerRequest* request) {},
		nullptr,
		[&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
			
			// Creiamo una stringa partendo dai dati ricevuti nel corpo del POST
			for (size_t i = 0; i < len; i++) {
				buffer_ser[i] = (char)data[i];
				if (i == len - 1) buffer_ser[len] = 0;
			}
			
			// prima cerco ed estraggo puntator client es:1070340744:DIR
			char* tokenNumero = strtok(buffer_ser, ":");
			
			if (tokenNumero != NULL) {
				// 2. Estrai il resto della frase (passando NULL a strtok)
				char* tokenFrase = strtok(NULL, "");  // "" legge tutto il resto fino alla fine
				
				// Converte il testo in un numero uint32_t (base 10)
				uint32_t puntatoreClient = strtoul(tokenNumero, NULL, 10);
				AsyncEventSourceClient* clientTmp = nullptr;
				
				//targetClient = nullptr;
				for (auto client : activeClients) {
					if ((uint32_t)client == puntatoreClient) {
						
						if (statoTask != 0) {
							clientTmp = client;
							//printWeb("SistemaOccupat0!!\n");
							clientTmp->send("SistemaOccupato!!\n", "serial_print", millis(), 0);
							return;
						}
						targetClient = client;
						break;
					}
				}
				if (targetClient == nullptr) {
					Serial.println(F("Errore CLIENT PTR"));
					return;
				}
				
				// Spostiamo la frase all'inizio di bufferInput.
				// Usiamo strlen(tokenFrase) + 1 per includere anche il carattere di fine stringa '\0'
				memcpy(buffer_ser, tokenFrase, strlen(tokenFrase) + 1);
				
				fromin = FROMWEB;  // dico che arriva da web
				statoTask = PARSING;
			}
			
			// Rispondi al client per chiudere la connessione HTTP correttamente
			request->send(200, "text/plain", "OK");
		}
	);
	
	// SSE connect
	eventsserial->onConnect([&](AsyncEventSourceClient* client) {
		
		Serial.println(F("--- [SSE] Un browser si è appena connesso! ---"));
		
		if (xSemaphoreTake(clientsMutex, portMAX_DELAY) == pdTRUE) {
			activeClients.push_back(client);
			targetClient = client;
			xSemaphoreGive(clientsMutex);
		}
		
		// TRUCCO: Convertiamo il puntatore della memoria in un ID numerico unico (uint32_t)
		uint32_t clientId = (uint32_t)client;
		// Generiamo una stringa con l'ID univoco assegnato dall'ESP32 a questo client (es. "ID_ASSEGNATO:1")
		//String infoId = "ID_ASSEGNATO:" + String(clientId);
		//client->send(infoId.c_str(), "message", millis(), 1000);
		char infoId[32];
		snprintf(infoId, sizeof(infoId), "ID_ASSEGNATO:%u", (unsigned int)(uintptr_t)client);
		client->send(infoId, "message", millis(), 1000);
		
		Serial.printf("Nuovo client. ID (RAM Address): %u. Totali: %d\n", clientId, activeClients.size());
		
		if (statoTask == 0) {
			fromin = FROMWEB;  // metto gia che dati arrivano da web per printWeb debug del codice
		}
		
	});
	
	// SSE disconnect
	eventsserial->onDisconnect([&](AsyncEventSourceClient* client) {
		
		// Questo metodo riceve nativamente il puntatore corretto quando un client si stacca
		
		
		if (xSemaphoreTake(clientsMutex, portMAX_DELAY) == pdTRUE) {
			// Cerchiamo il puntatore direttamente nel vettore
			auto it = std::find(activeClients.begin(), activeClients.end(), client);
			if (it != activeClients.end()) {
				activeClients.erase(it);
			}
			// Se nel vettore è rimasto almeno un altro client attivo, lo promuoviamo a nuovo target
			if (!activeClients.empty()) {
				targetClient = activeClients.front();  // .front() prende il primo client disponibile nella lista
				//Serial.printf("Nuovo targetClient assegnato automaticamente: %u\n", (uint32_t)targetClient);
				} else {
				// Se non è rimasto più nessuno, resettiamo a nullptr
				targetClient = nullptr;
				//Serial.println("Non ci sono altri client disponibili. targetClient resettato a NULL.");
			}
			xSemaphoreGive(clientsMutex);
		}
		
		Serial.printf("[SSE] Client %u disconnesso. Rimasti: %d\n", (uint32_t)client, activeClients.size());
		
		if (activeClients.size() > 0) {
			eventsserial->send(String(activeClients.size()), "client_count", millis(), 0);
		}
		
	});
	
	server->addHandler(eventsserial);
	//server->begin();
	
	Serial.println(F("WebSerialSim avviato"));
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

    if (!fullbuffer) return;

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
}


void WebSerialSim::modestory(bool action) {
	
	if(action){
		if(attivaBufferPSRAM())actSerBuf = true;
		}else{
		actSerBuf = false;
	}
	
	if(!action){
		actSerBuf = false;
		delay(5);
		if (historySerBuf){
			actSerBuf = false;
			fHistoryFlush();
			free(historySerBuf);
			historySerBuf = nullptr;
		}
	}
	infoSerBuf();
}

void WebSerialSim::setbuffer(size_t _dimbuffer) {
	dimSerBuf = _dimbuffer;
	if(_dimbuffer != 0 && _dimbuffer 1500)
		Serial.println(F("Attenzione buffer too small ..almeno 1500"));
	modestory(true);
}

void WebSerialSim::parsinghistory(char *opzion) {
	//Serial.println(opzion);
	
	if (strstr(opzion, "ON") != NULL) {
    modestory(true);
    //printWeb("-HISTORY ATTIVO\n");
		} else if (strstr(opzion, "OFF") != NULL) {
    modestory(false);
    //printWeb("-HISTORY disattivato\n");
		} else if (strstr(opzion, "VIEW") != NULL) {
    fViewHistory();
		} else if (strstr(opzion, "CLEAR") != NULL) {
    fHistoryClear();
		} else if (strstr(opzion, "FLUSH") != NULL) {
    fHistoryFlush();
		} else if (strstr(opzion, "INFO") != NULL) {
    infoSerBuf();
		} else {
    printWeb("-OPZIONE non valida!\n");
	}
	
}


bool WebSerialSim::attivaBufferPSRAM() {
	
		
		if (historySerBuf != nullptr) {
			Serial.print(F("-BUFFER già attivo\n"));
			return true;
		}
		
		pSerBuf = 0;
		
		if(dimSerBuf == 0){
			#ifdef _TYPE_FS
			directFS = true;
			#endif
			return true;
		}
		
		
		if(inPSRAM){
			
			if(!psramFound()){
				Serial.println(F("-PSRAM NON RILEVATA o DISABILITATA"));
				inPSRAM = false;
				}else {
				historySerBuf = (char*)ps_malloc(dimSerBuf + 1);
			}
			
			}else{
			historySerBuf = (char *) malloc(dimSerBuf + 1);
		}
		
		
		if (!historySerBuf) {
			printWeb("-ERRORE allocazione Buffer HISTORY\n");
			actSerBuf = false;
			dimSerBuf = 0;
			return false;
		}
		
		memset(historySerBuf, 0, dimSerBuf + 1);
		printWeb("-HISTORY buffer creato\n");
		actSerBuf = true;
		
		return true;
		
}

void WebSerialSim::fHistoryClear() {
	
	bool riaccendi = actSerBuf;
	actSerBuf = false;
	if (!historySerBuf) {
		printWeb("-BUFFER non presente\n");
		actSerBuf = riaccendi;
		return;
	}
	
	historySerBuf[0] = '\0';
	pSerBuf = 0;
	fullbuffer = false;
	actSerBuf = riaccendi;
}

void WebSerialSim::fHistoryFlush() {
	#ifdef _TYPE_FS
	bool riaccendi = actSerBuf;
	actSerBuf = false;
	
	if (!historySerBuf) {
		printWeb("-BUFFER non presente\n");
		actSerBuf = riaccendi;
		return;
	}
	
	if (pSerBuf > 0){
		// ci sono dati nel buffer da aggiornare
		File hfile = FS_STORY.open(FILE_HISTORY, FILE_APPEND);
		hfile.write((const uint8_t*)historySerBuf, pSerBuf);
		hfile.close();
	}
	
	pSerBuf = 0;
	actSerBuf = riaccendi;
	#endif
}

void WebSerialSim::insHistory(const char* str){
	
	int dimstr = strlen(str);
	// per sicurezza se la str e' piu lunga del buffer ..tronco
	// accertarsi sempre che il buffer sia almeno 1500 byte
	if(dimstr >= dimSerBuf) dimstr = dimSerBuf -1;
	
	#ifdef _TYPE_FS
	if(directFS && actSerBuf){
		File hfile = FS_STORY.open(FILE_HISTORY, FILE_APPEND);
		hfile.write((const uint8_t*)str, dimstr);
		hfile.close();
		return;
	}
	#endif
	
	if(!actSerBuf || !historySerBuf) return;
	
  int spazio_alla_fine = dimSerBuf - pSerBuf;
	
	if (dimstr <= spazio_alla_fine) {
    // Caso 1: La stringa ci sta tutta di seguito fino alla fine del buffer
    memcpy(&historySerBuf[pSerBuf], str, dimstr);
    pSerBuf += dimstr;
		if (pSerBuf == dimSerBuf){
			pSerBuf = 0;
			fullbuffer = true;
		}
		//if(!fullbuffer) historySerBuf[pSerBuf + dimstr] = '\0';
		
		} else {
    // Caso 2: La stringa si deve spezzare in due (una parte alla fine, il resto all'inizio)
    int prima_parte = spazio_alla_fine;
    int seconda_parte = dimstr - spazio_alla_fine;
		
    memcpy(&historySerBuf[pSerBuf], str, prima_parte);
		
		// rec su sd prima di sovrascrivere ------------------
		#ifdef _TYPE_FS
		if(!directFS) fregbuffer();
		#endif
		
    memcpy(historySerBuf, &str[prima_parte], seconda_parte);
		
    pSerBuf = seconda_parte;
    fullbuffer = true; // Il buffer ha completato almeno un giro completo
	}
	
}


void WebSerialSim::infoSerBuf() {
	
	bool riaccendi = actSerBuf;
	actSerBuf = false;
	
	#ifdef _TYPE_FS
		printfWeb("-Funzione LOG File Attiva su %s\n", _TYPE_FS);
		#else
		printWeb("-Funzione LOG File NON attiva\n");
		//actSerBuf = riaccendi;
		//return;
	#endif
	
	/* #ifdef _TYPE_FS
		printfWeb("- File su %s\n", _TYPE_FS);
	#endif */
	
	if (!historySerBuf) {
		printWeb("-BUFFER non presente\n");
		} else {
		printfWeb("-Dimensione buffer: %u byte", dimSerBuf);
		printfWeb("-Occupati: %u byte", fullbuffer ? dimSerBuf : pSerBuf);
		printfWeb("-Buffer in %s\n", (inPSRAM) ? "PSRAM" : "SRAM");
	}
	
	#ifdef _TYPE_FS
	if(directFS) printWeb("-LOG DIRETTO su File..\n");
	#endif
	printfWeb("-STATO %s..\n", riaccendi ? "RUN" : "PAUSA");
		
	actSerBuf = riaccendi;
}

void WebSerialSim::fregbuffer() {
	#define oldstory "/oldstory.txt"
	//#ifdef HISTORY_FS
	#ifdef _TYPE_FS
		
		File h = FS_STORY.open(FILE_HISTORY, FILE_APPEND);
		if(!h) return;
		bool riaccendi = actSerBuf;
		actSerBuf = false;
		
		if (pSerBuf > 0) {
			h.write((const uint8_t*)historySerBuf, dimSerBuf);
		}
		
		actSerBuf = riaccendi;
		
		if (h && h.size() > MAXSIZEFILE_HISTORY) {
			h.close();
			FS_STORY.remove(oldstory);
			FS_STORY.rename(FILE_HISTORY, oldstory);
			} else if (h) {
			h.close();
		}
	#endif
}

void WebSerialSim::fViewHistory() {
	
	bool riaccendi = actSerBuf;
	actSerBuf = false;
	
	if (!historySerBuf || (!fullbuffer && pSerBuf == 0)) {
		printWeb("-BUFFER non presente o vuoto..\n");
		actSerBuf = riaccendi;
		return;
	}
	
  printWeb("\n--- CONTENUTO DEL BUFFER ---\n");
	
  if (fullbuffer) {
		// Se è pieno, stampa prima dalla posizione attuale alla fine del buffer (dati vecchi)
		printBigBuf(&historySerBuf[pSerBuf], dimSerBuf - pSerBuf);
	}
	
  // Stampa dall'inizio del buffer fino alla posizione attuale (dati nuovi)
  if (pSerBuf > 0) {
		printBigBuf(historySerBuf, pSerBuf);
	}
	printWeb("\n----------------------------\n");
	
	actSerBuf = riaccendi;
}
