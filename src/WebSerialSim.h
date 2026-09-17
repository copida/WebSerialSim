#ifndef WEB_SERIAL_SIM_H
#define WEB_SERIAL_SIM_H
	
	#include <Arduino.h>
	#include <ESPAsyncWebServer.h>
	//#include <vector>
	//#include <algorithm>
	#include <esp_psram.h>
	#include "htmlserialSim.h"			// paguna HTML
	
	
	#define MAXSIZEBUFFER_HISTORY 4000
	#define CHUNK_SIZE 1440 // Ottimizzato sotto la soglia MTU standard (1460)
	
	#define FILE_HISTORY "/history.txt"
	#define MAXSIZEFILE_HISTORY 512000
	
	#define HISTORY_SD
	//#define HISTORY_SDMMC
	//#define HISTORY_LittleFS
	
	#ifdef HISTORY_SD
		#include <SD.h>
		#define FS_STORY SD
		#define _TYPE_FS "SD"
		#pragma message "### Serial WEB SIM History su 'SD' ###"
	#endif
	
	#ifdef HISTORY_SDMMC
		#include <SD_MMC.h>
		#define FS_STORY SD_MMC
		#define _TYPE_FS "SD_MMC"
		#pragma message "### Serial WEB SIM History su 'SD MMC' ###"
	#endif
	
	#ifdef HISTORY_LittleFS
		#include <FS.h>
		#include <LittleFS.h>
		#define FS_STORY LittleFS
		#define _TYPE_FS "LittleFS"
		#pragma message "### Serial WEB SIM History su 'LittleFS' ###"
	#endif
	
	
	using CallbackFunzione = void (*)(char*);
	using CallbackBLE = void (*)(char*);
	
	
	// Origine comando
	enum From {
		NOTOUT,
		FROMSER,
		FROMWEB,
		FROMBT
	};
	
	// Stato processore
	enum StatoProc {
		IDLE,
		PARSING,
		WORKING
	};
	
	#define LEN_BUF_SER 60
	#define PRINTF_LENMAX 256
	
	
	class WebSerialSim : public Print {
		
		public:
		WebSerialSim();
		~WebSerialSim();
		
		// =========================================================================
		// METODI VIRTUALI DI PRINT (Sostituiscono i vecchi template / String)
		// =========================================================================
		
		virtual size_t write(uint8_t c) override;
		virtual size_t write(const uint8_t *buffer, size_t size) override;
		
		using Print::write;
		// =========================================================================
		
		void printfWeb(const char* format, ...);
		// NUOVA: Versione per stringhe racchiuse nella macro F()
		//void printfWeb(const  __FlashStringHelper* format, ...);
		
		
		void printWeb(char* _datiprint, size_t quantsize = 0);
		void sendWeb(char* _dati, size_t len);
		
		// HISTORY RAM
		void modestory(bool action);
		void setPSRAM(bool _enable);
		bool makeBuffer();
		void setbuffer(size_t _dimbuffer);
		void setHistoryFile(bool enable);
		bool inPSRAM = true;
		void infoSerBuf();
		void fregbuffer();
		void fHistoryFlush();
		void fHistoryLoad();
		void insHistory(const char* str);
		void reverse(char* buf, size_t start, size_t end);
		void unrollBuffer();
		void parsinghistory(char *opzion);
		void printBigBuf(char *bigbuf, size_t dim = 0);
		// ===== TIMESTAMP =====
		bool enableTimestamp = false;
		void setTimestampEnabled(bool enable) { enableTimestamp = enable; }
		char* getTimestampString();
		
		// SSE
		
		bool checkClientSSE();
		bool canSendSSE(size_t requiredSpace);
		
		// Task
		void taskList();
		void readserial(char* buffer_ch, int max_ch);
		
		// Comandi
		bool inputEXT(char* inExt, int lenb);
		void parsingCmd();
		void echoOnOff(bool onoff);
		void switchState(int fasestate);
		void handleBufferIn();
		
		void begin(AsyncWebServer* mainServer);
		
		// Metodo per registrare la funzione esterna
		// Funzione per impostare il puntatore alla funzione dello sketch
    void setCallback(CallbackFunzione cb);
		void setCallBLE(CallbackBLE cb);
		
		
		private:
		
		// Server interno
		AsyncWebServer* server;
		AsyncEventSource* eventsserial;
		
		// Client SSE
		//std::vector<AsyncEventSourceClient*> activeClients;
		//SemaphoreHandle_t clientsMutex;
		//AsyncEventSourceClient* targetClient;
		AsyncEventSourceClient* clientSSEGlobale;
		
		// Buffer PSRAM o SRAM
		bool historyFileEnabled = true;
		char* historySerBuf;
		bool stateRun;
		size_t pSerBuf;
		size_t tailBuf;
		size_t dimSerBuf;
		size_t tmpdimSerBuf;
		bool fullbuffer;
		bool directFS;
		char _timestamp[15];		// "[HH:MM:SS] " = 11 char max
		char chunkBuf[CHUNK_SIZE];
		
		// Stato
		char buffer_ser[LEN_BUF_SER];
		char* command;
		char* argument;
		bool echon;
		uint16_t fromin;
		int statoTask;
		
		// buffer per write
		//#define DIMBUFFERIN 100
		#define DIMBUFFERIN 600
		char bufferIn[DIMBUFFERIN];
		size_t bufIndexIn = 0;
		unsigned long ultimoCarattereTime = 0;
		const unsigned long TIMEOUT_MS = 40; // Tempo di attesa prima dell'invio forzato
		
		
		// Callback esterna
		CallbackFunzione _callback; // Puntatore interno alla funzione esterna
		CallbackBLE _callBLE;		// puntatore per output su ble o altro..
		
	};
	
#endif
