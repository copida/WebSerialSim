#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);

#include "WebSerialSim.h"
WebSerialSim serialWeb;

static uint32_t last = millis();
static uint32_t count = 0;

const char* ssid = "XXXXXXXXXXXXXXXXXXX";
const char* password = "XXXXXXXXXXXXXXXX";

void onCbReceive(char* cmd);
void listDir();

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
  } else {
    serialWeb.println(cmd);
    Serial.print("NON trovato:");
  }
}


void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.begin(ssid, password);

  Serial.print("Connessione in corso");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnesso!");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());

  // Verifica se la PSRAM è attiva e funzionante sul modulo
  if (!psramInit()) {
    Serial.println("ATTENZIONE: PSRAM non rilevata o non funzionante!");
  } else {
    Serial.printf("[PSRAM] Rilevata. Memoria libera in PSRAM: %d bytes\n", ESP.getFreePsram());
  }

  if (!SD.begin(46)) {
    Serial.println("SD...FAIL");
  } else {
    Serial.println("SD...OK");
  }

  //// **************************************************************
  //// *  DEFAULT
  //// *  Con valori di default:
  //// *  Buffer 4000 bytes
  //// *  History enable (history.txt)
  //// **************************************************************
  serialWeb.setCallback(onCbReceive);
  serialWeb.begin(&server);  // crea server interno

  //// **************************************************************
  //// *  Diretto no History No buffer
  //// *  Buffer 0 bytes
  //// *  History disable
  //// *  CONFIG 0
  //// *  CONFIG NOFS
  //// **************************************************************
  // serialWeb.setCallback(onCbReceive);
  // serialWeb.setbuffer(0);
  // serialWeb.setHistoryFile(false);
  // serialWeb.begin(&server);  // crea server interno

  //// **************************************************************
  //// *  Diretto su History file No buffer
  //// *  Buffer 0 bytes
  //// *  History enable
  //// *  CONFIG 0
  //// *  CONFIG FS
  //// **************************************************************
  // serialWeb.setCallback(onCbReceive);
  // serialWeb.setbuffer(0);
  // serialWeb.setHistoryFile(true);
  // serialWeb.begin(&server);  // crea server interno

  //// **************************************************************
  //// *  SOLO buffer  NO history
  //// *  Buffer 1000000 bytes...
  //// *  History disable
  //// *  CONFIG 1000000
  //// *  CONFIG NOFS
  //// **************************************************************
  // serialWeb.setCallback(onCbReceive);
  // serialWeb.setbuffer(1048576);
  // serialWeb.setHistoryFile(false);
  // serialWeb.begin(&server);  // crea server interno
  //// **************************************************************

  server.onNotFound([](AsyncWebServerRequest* request) {
    request->redirect("/serial");
  });

  server.begin();
}

void loop() {
  serialWeb.taskList();
  
  if (millis() - last > 5000) {
    count++;
    uint32_t ms = millis();

    serialWeb.println(ms);
    serialWeb.printf("Messaggio n: %d\n", count);

    last = millis();
  }
}

//=============================
void listDir() {

  File root = SD.open("/");
  if (!root) {
    serialWeb.println("Failed to open directory");
    return;
  }

  serialWeb.printf("Listing directory: %s\n", "/");
  serialWeb.print("\======= START ==============\n");

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      serialWeb.printf("DIR  %s\n", file.name());
    } else {
      serialWeb.printf("%-18s Size   %d byte\n", file.name(), file.size());
    }
    file = root.openNextFile();
  }
  serialWeb.print("\n======= END ===============\n");
}
