# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-10-02

### Initial Release

A complete, production-ready serial-to-web bridge for ESP32 embedded systems.

#### Added

- **Serial streaming via SSE** - Real-time log delivery to web browsers
- **Circular history buffer** - Configurable RAM/PSRAM/SD storage with intelligent wraparound
- **Command parsing** - Runtime commands from Serial, HTTP, or external callbacks
- **Timestamp support** - Real-time or lightweight uptime modes
- **Multi-backend storage** - SD, SD_MMC, LittleFS, SdFat support
- **File persistence** - Automatic history flush to filesystem
- **Web dashboard** - Built-in view/download interface at `/serial`
- **Print interface** - Drop-in replacement for Serial, inherits from Arduino Print class
- **BLE callbacks** - Optional Bluetooth integration support
- **Non-blocking design** - Optimized for FreeRTOS multitask environments
- **High performance** - Validated with mega-bytes of sequential streaming

#### Features

- ✅ Handles 1MB+ continuous logs without data loss
- ✅ Intelligent chunking to respect MTU limits
- ✅ Retry logic with backoff for SSE delivery
- ✅ Thread-safe history buffer management with mutex protection
- ✅ Flexible configuration via compile-time defines
- ✅ Graceful fallback for slow or disconnected clients

#### Fixed

- Thread-safety improvement: replaced `strtok()` with `strchr()` for deterministic parsing
- Underflow protection: added validation for zero-sized buffers
- Timestamp generation: improved thread-safety with buffer protection

#### Technical notes

- Uses ESPAsyncWebServer and SSE instead of WebSocket for reliability
- Ring buffer pattern for bounded memory usage
- Atomic file I/O operations for persistence
- FreeRTOS mutex protection for concurrent access
- Supports both real-time and uptime-based timestamping

#### Known limitations

- Single-client SSE connection (by design)
- Parsing not recommended for highly concurrent multitask scenarios without isolation
- Timestamp accuracy depends on `time()` availability for real-time mode

#### Testing

- Validated with continuous high-throughput logging (mega-bytes per session)
- Tested on ESP32 with PSRAM and SD storage
- Confirmed stable operation in FreeRTOS multitask environments

---

## Versioning

This project follows [Semantic Versioning](https://semver.org/):

- **MAJOR** - breaking API changes or significant architectural shifts
- **MINOR** - new features or non-breaking enhancements
- **PATCH** - bug fixes and minor improvements

---

## Future releases

Future versions may include:

- improved command validation and error handling
- richer diagnostics and statistics
- enhanced web UI
- broader filesystem backend support
- better memory efficiency optimizations

---

## Support

For issues, questions, or feedback, please open an issue on the [GitHub repository](https://github.com/copida/WebSerialSim/issues).
