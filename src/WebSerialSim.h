#ifndef WEB_SERIAL_SIM_H
#define WEB_SERIAL_SIM_H
	
	#include <Arduino.h>
	#include <ESPAsyncWebServer.h>
	
	#define INTERNALHTML
	#define PSRAM
	
	#ifdef INTERNALHTML
	#define GZIP
	#ifdef GZIP
	#include "htmlfile.h"			// pagina HTML GZIP
	#else
	#include "htmlserialSim.h"			// paguna HTML
	#endif
	#endif
	
	
	#define MAXSIZEBUFFER_HISTORY 4000
	#define CHUNK_SIZE 1440 // Ottimizzato sotto la soglia MTU standard (1460)
	
	#define FILE_HISTORY "/history.txt"
	#define MAXSIZEFILE_HISTORY 512000
	
	#define HISTORY_SD
	//#define HISTORY_SDMMC
	//#define HISTORY_LittleFS
	//#define HISTORY_sdFAT
	
	#ifdef HISTORY_SD
		#include <SD.h>
		#define FS_DRV SD
		typedef File FS_FILE_TYPE;
		#define _TYPE_FS "SD"
		#define MOD_APPEND FILE_APPEND
		#define MOD_READ FILE_READ
		#pragma message "### Serial WEB SIM History su 'SD' ###"
	#endif
	
	#ifdef HISTORY_SDMMC
		#include <SD_MMC.h>
		#define FS_DRV SD_MMC
		typedef File FS_FILE_TYPE;
		#define _TYPE_FS "SD_MMC"
		#define MOD_APPEND FILE_APPEND
		#define MOD_READ FILE_READ
		#pragma message "### Serial WEB SIM History su 'SD MMC' ###"
	#endif
	
	#ifdef HISTORY_LittleFS
		#include <FS.h>
		#include <LittleFS.h>
		#define FS_DRV LittleFS
		typedef File FS_FILE_TYPE;
		#define _TYPE_FS "LittleFS"
		#define MOD_APPEND FILE_APPEND
		#define MOD_READ FILE_READ
		#pragma message "### Serial WEB SIM History su 'LittleFS' ###"
	#endif
	
	#ifdef HISTORY_sdFAT
		#include <SPI.h>
		#include <SdFat.h>
		extern SdFat sd; // Dichiarata nel file .ino o .cpp principale
		#define FS_DRV sd
		typedef FsFile FS_FILE_TYPE;
		#define _TYPE_FS "sd"
		#define MOD_APPEND (O_WRITE | O_CREAT | O_APPEND)
		#define MOD_READ O_READ
		#pragma message "### Serial WEB SIM History su 'SdFat' ###"
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
		
		
	
		void playstory(bool action);
		void setPSRAM(bool _enable);
		
		void setbuffer(size_t _dimbuffer);
		void setHistoryFile(bool enable);
		void setTimestamp(bool enable);
		bool inPSRAM = true;
		void infoSerBuf();
		
		// ===== TIMESTAMP =====
		bool enableTimestamp = false;
		void setTimestampEnabled(bool enable) { enableTimestamp = enable; }
		void getTimestampString();
	
		
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
		
    void setCallback(CallbackFunzione cb);
		void setCallBLE(CallbackBLE cb);
		
// PRIVATE =========================================================================
		private:
		
		// Helper RAII privato
    class AutoLock {
    private:
        SemaphoreHandle_t _sem;
    public:
        AutoLock(SemaphoreHandle_t sem, TickType_t waitTicks = portMAX_DELAY) : _sem(sem) {
            if (_sem != nullptr) {
                if (xSemaphoreTake(_sem, waitTicks) != pdTRUE) {
                    _sem = nullptr;
                }
            }
        }
        ~AutoLock() {
            if (_sem != nullptr) xSemaphoreGive(_sem);
        }
        bool isLocked() const { return _sem != nullptr; }
    };

    SemaphoreHandle_t _mutex;
		
		// Server interno
		AsyncWebServer* server;
		AsyncEventSource* eventsserial;
		
		// Client SSE
		//std::vector<AsyncEventSourceClient*> activeClients;
		//SemaphoreHandle_t clientsMutex;
		//AsyncEventSourceClient* targetClient;
		AsyncEventSourceClient* clientSSEGlobale;
		AsyncClient* rawClient;
		
		// SSE
		
		//bool checkClientSSE();
		//bool canSendSSE(size_t requiredSpace);
		bool txSSE(char* _datiprint, size_t requiredSpace, bool txtimestamp = false);
		uint16_t _nerrTX;
		
		void fregbuffer();
		void fHistoryFlush();
		void fHistoryLoad();
		void insBuffer(const char* str);
		void reverse(char* buf, size_t start, size_t end);
		void unrollBuffer();
		void parsinghistory(char *opzion);
		void printWeb(char *bigbuf, size_t dim = 0);
		
		void printfWeb(const char* format, ...);
		// NUOVA: Versione per stringhe racchiuse nella macro F()
		//void printfWeb(const  __FlashStringHelper* format, ...);
		
		
		//void printWeb(char* _datiprint, size_t quantsize = 0);
		void sendWeb(char* _dati, size_t len);
		
		
		bool makeBuffer();
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
		char _timestamp[15] = {"[00:00:00] "};		// "[HH:MM:SS] " = 11 char max
		char chunkBuf[CHUNK_SIZE];
		
		// Stato
		char buffer_ser[LEN_BUF_SER];
		char* command;
		char* argument;
		bool echon;
		uint16_t fromin;
		int statoTask;
		//int max_mtu;
		
		// buffer per write
		//#define DIMBUFFERIN 100
		#define DIMBUFFERIN 600
		char bufferIn[DIMBUFFERIN];
		size_t bufIndexIn = 0;
		unsigned long ultimoCarattereTime = 0;
		const unsigned long TIMEOUT_MS = 40; // Tempo di attesa prima dell'invio forzato
		
		// Callback esterna (puntatori)
		CallbackFunzione _callback;
		CallbackBLE _callBLE;
	};
	
#endif
