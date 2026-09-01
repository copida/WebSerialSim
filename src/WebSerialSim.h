#ifndef WEB_SERIAL_SIM_H
	#define WEB_SERIAL_SIM_H
	
	#include <Arduino.h>
	#include <ESPAsyncWebServer.h>
	#include <vector>
	#include <algorithm>
	#include <esp_psram.h>
	#include "htmlserialSim.h"
	
	#define HISTORY_FS 1
	#define MAXSIZEFILE_HISTORY 512000
	// se MAXSIZEBUFFER_HISTORY 0 allora la scrittura è diretta senza buffer (più lento)
	#define MAXSIZEBUFFER_HISTORY 4000
	//#define MAXSIZEBUFFER_HISTORY 0
	#define BUFFER_PSRAM
	
	#if HISTORY_FS
		
		#define FILE_HISTORY "/history.txt"
		
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
		
	#endif
	
	using CallbackFunzione = void (*)(char*, char*);
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
	
	// Eredita direttamente da Print
	class WebSerialSim : public Print {
		
		public:
		WebSerialSim();
		~WebSerialSim();
		
		// =========================================================================
		// METODI VIRTUALI DI PRINT (Sostituiscono i vecchi template / String)
		// =========================================================================
		// Scrittura singolo carattere (obbligatorio per ereditare da Print)
		virtual size_t write(uint8_t c) override;
		
		// Scrittura a blocco (fondamentale per stringhe, printf e array di char)
		virtual size_t write(const uint8_t *buffer, size_t size) override;
		
		// Rende visibili anche gli altri sovraccarichi di write della classe base
		using Print::write;
		// =========================================================================
		
		void printfWeb(const char* format, ...);
		void printWeb(char* _datiprint);
		void sendWeb(char* _dati, size_t len);
		
		// HISTORY RAM
		void modestory(bool action);
		bool attivaBufferPSRAM();
		void setbuffer(size_t _dimbuffer);
		bool inPSRAM = true;
		void infoSerBuf();
		void fViewHistory();
		void fregbuffer();
		void fHistoryClear();
		void fHistoryFlush();
		void insHistory(const char* str);
		void reverse(char* buf, size_t start, size_t end);
		void unrollBuffer();
		void parsinghistory(char *opzion);
		void printBigBuf(char *bigbuf, size_t dim = -1);
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
		std::vector<AsyncEventSourceClient*> activeClients;
		SemaphoreHandle_t clientsMutex;
		AsyncEventSourceClient* targetClient;
		
		// Buffer PSRAM o SRAM
		char* historySerBuf;
		bool actSerBuf;
		size_t pSerBuf;
		//size_t pSerBufOld;
		size_t dimSerBuf;
		bool fullbuffer;
		bool directFS;
		//bool inPSRAM;
		//File hfile;
		
		// Stato
		char buffer_ser[LEN_BUF_SER];
		char* comando;
		char* param1;
		bool echon;
		uint16_t fromin;
		int statoTask;
		
		// buffer per write
		#define DIMBUFFERIN 100
		char bufferIn[DIMBUFFERIN];
		size_t bufIndexIn = 0;
		unsigned long ultimoCarattereTime = 0;
		const unsigned long TIMEOUT_MS = 50; // Tempo di attesa prima dell'invio forzato
		
		// Password executive
		//char* pwd_executive;
		
		// Callback esterna
		CallbackFunzione _callback; // Puntatore interno alla funzione esterna
		CallbackBLE _callBLE;		// puntatore per output su ble o altro..
		
		// Utility
		//void appbuf(const char* str, char* destBuf, size_t maxDim, size_t* pIndice);
	};
	
#endif
