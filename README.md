# 🌐 WebSerialSim

> High-performance serial-to-web bridge for ESP32 and embedded debugging workflows

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![ESP32](https://img.shields.io/badge/platform-ESP32-success.svg)](https://www.espressif.com/products/microcontrollers/esp32/overview)
[![AsyncWebServer](https://img.shields.io/badge/library-ESPAsyncWebServer-orange.svg)](https://github.com/me-no-dev/ESPAsyncWebServer)

WebSerialSim provides a robust web-based serial monitor for embedded systems. It streams Serial output to a browser using SSE, captures history in RAM/PSRAM/SD, supports command parsing, and allows custom callbacks for remote control and diagnostics.

---

## ✨ Why WebSerialSim?

Embedded debugging is often limited to a local USB console. WebSerialSim solves that by turning your device into a remote serial monitor over WiFi.

### What it is

- a lightweight serial terminal over HTTP
- a live stream of logs to the browser
- a history buffer with optional file persistence
- a command processor for runtime control
- a drop-in Print-based interface that works like Serial

### Why this approach

| Aspect | Serial Monitor | WebSerialSim |
|--------|---------------|--------------|
| Remote access | ❌ | ✅ |
| Browser access | ❌ | ✅ |
| Persistent history | ❌ | ✅ |
| Live log streaming | ✅ | ✅ |
| Buffer management | ❌ | ✅ |
| Command parsing | ❌ | ✅ |
| Remote diagnostics | ❌ | ✅ |

Designed for ESP32 firmware debugging, telemetry, and long-running device inspection.

---

## 🚀 Features

- Serial-to-web bridge over SSE
- Remote command execution
- Circular history buffer in RAM / PSRAM / filesystem
- Configurable timestamping
- File-based history storage for persistence
- Built-in web view and download routes
- Callback interfaces for custom logic or BLE
- Works as a `Print` subclass, so it behaves like standard Serial in many cases

---

## 🧩 Supported backends

WebSerialSim supports different storage backends depending on your build configuration:

- `SD`
- `SD_MMC`
- `LittleFS`
- `SdFat`

It can also use PSRAM when available, with graceful fallback to SRAM.

---

## 🔌 Quick start

### Installation

Add the library to your project:

```cpp
#include <WebSerialSim.h>
```

Or copy the files from `src/` into your Arduino/PlatformIO project.

### Minimal example

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "WebSerialSim.h"

AsyncWebServer server(80);
WebSerialSim serialSim;

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("WebSerialSim", "12345678");

  serialSim.begin(&server);
  server.begin();
}

void loop() {
  serialSim.taskList();
  delay(1);
}
```

Then open:

```text
http://<device-ip>/serial
```

---

## 🧠 Runtime commands

The library accepts commands from Serial, HTTP, or external input.

### Timestamp control

```text
TIMESTAMP ON
TIMESTAMP OFF
```

### History control

```text
HISTORY ON
HISTORY OFF
HISTORY CLEAR
HISTORY INFO
HISTORY LOAD
```

### Configuration

```text
CONFIG FS
CONFIG NOFS
CONFIG PSRAM
CONFIG NOPSRAM
CONFIG 4000
```

Examples:
- enable file logging
- disable file logging
- set buffer size to 4000 bytes
- toggle PSRAM usage

---

## 🌐 HTTP routes

| Route | Method | Purpose |
|--------|--------|---------|
| `/serial` | GET | Web UI for live monitoring |
| `/events/serial` | GET | SSE serial stream |
| `/buffer?action=view` | GET | View stored history |
| `/buffer?action=down` | GET | Download history |
| `/delhistory` | GET | Remove history file |
| `/parsingCmd` | POST | Send command payload |

---

## ⏱️ Timestamp modes

The library supports two timestamp strategies:

### Real-time mode

```cpp
#define TIMESTAMP_REALTIME
```

Uses `time()` and wall-clock time.

### Uptime mode

Default behavior uses `millis()` to generate a lightweight time value without extra RTC setup.

This is useful when:
- you want very low overhead
- the device is not synchronized to a real time source
- you want timestamp generation with minimal CPU cost

---

## 💾 Storage behavior

History can be stored in:
- RAM
- PSRAM
- filesystem-backed storage
- direct-to-file mode when buffer is disabled

The library uses a circular buffer pattern and flushes history intelligently before overwrite.

### Example

```cpp
webSerial.setbuffer(4096);
webSerial.setHistoryFile(true);
webSerial.setPSRAM(true);
```

---

## 🧪 Performance notes

The implementation is designed for high-throughput serial monitoring.

Validated characteristics include:
- sustained high-volume serial output
- SSE-based delivery without blocking the main loop
- large sequential log streams without requiring WebSocket infrastructure
- stable operation in embedded multitask workflows

---

## ⚙️ API overview

### Main public methods

```cpp
void begin(AsyncWebServer* mainServer);
void taskList();
void setCallback(CallbackFunzione cb);
void setCallBLE(CallbackBLE cb);
void setbuffer(size_t bytes);
void setHistoryFile(bool enable);
void setPSRAM(bool enable);
void setTimestamp(bool enable);
void echoOnOff(bool onoff);
void infoSerBuf();
```

`WebSerialSim` also derives from `Print`, so it supports standard print operations.

---

## 🧾 Example callback

```cpp
void handleCommand(char* cmd) {
  Serial.printf("CMD: %s\n", cmd);
}

void setup() {
  webSerial.setCallback(handleCommand);
}
```

---

## 🛠️ Why SSE and not WebSocket?

For a serial monitor, the output is mostly unidirectional: device → browser. SSE is a better fit because it is:

- simpler
- lighter
- easier to integrate with HTTP server routing
- reliable for large continuous logs
- well-suited to browser streaming without binary protocol overhead

This keeps the implementation smaller and more stable for continuous log monitoring.

---

## 🐛 Troubleshooting

### No logs appear in browser

- verify WiFi access point is running
- confirm `/serial` route is served
- make sure `server.begin()` is called
- check Serial output from the ESP32 itself

### History not saving

- check filesystem backend is enabled
- ensure adequate free memory
- run `HISTORY INFO` to inspect state

### Buffer too small

```text
Attenzione buffer too small ..almeno 1500
```

Increase the history buffer size using:

```cpp
webSerial.setbuffer(4096);
```

---

## 📘 Design notes

The library intentionally separates responsibilities:

- `insBuffer()` handles history accumulation into the memory buffer
- `fregbuffer()` handles snapshot persistence to filesystem
- `begin()` sets up the web routes and SSE stream
- `taskList()` keeps the internal state machine active

This keeps the code modular and easier to maintain in embedded firmware.

---

## 🧾 License

MIT License.

See the [LICENSE](LICENSE) file for details.

---

## 👀 Roadmap

Planned enhancements are focused on stability and usability rather than protocol churn:

- improved command validation
- more detailed buffer diagnostics
- richer web UI panels
- broader filesystem backend coverage
- better release packaging for library usage

---

## ❤️ Project status

WebSerialSim is built for practical firmware diagnostics and field debugging. It aims to give embedded developers a simple, reliable way to inspect device output remotely without adding heavy infrastructure.

If you need a browser-based serial monitor for ESP32 projects, this library is designed to be a fast and lightweight option.
