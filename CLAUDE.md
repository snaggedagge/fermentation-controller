# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP32 fermentation temperature controller (PlatformIO, Arduino framework). A DS18B20 probe is read, a cooling and a heating relay are switched with hysteresis around a target temperature, and state is shown on an ST7789 TFT and a WiFi web UI. There are no automated tests and no linter configured.

## Commands

```bash
pio run                          # build (env: esp32dev)
pio run --target upload          # flash; platformio.ini uses upload_protocol=espota to a fixed IP (upload_port)
pio run --target uploadfs        # upload data/ (index.html) to LittleFS - required after editing the web UI
pio device monitor               # serial at 115200 with exception decoder
```

- Uploads are OTA by default. For first flash or when the device is unreachable, override with e.g. `--upload-port COMx` and `upload_protocol = esptool` in platformio.ini.
- The web UI lives in `data/index.html` and is served from LittleFS, so UI changes need `uploadfs`, not a firmware upload.
- `test-data/*.json` (1h/6h/24h) are sample `/history` payloads, useful for developing the UI without hardware.

## Architecture

`src/main.cpp` owns global instances of three classes and a cooperative `loop()` (no RTOS tasks of its own):

- `TemperatureController` — DS18B20 reads, relay control, target temp persisted via `Preferences` (NVS, namespace `temp_ctrl`), and a 24h ring buffer of temperature/relay-state history (`HISTORY_COUNT`, downsampled to `HISTORY_POINTS` for the web). Pins, hysteresis, target range and history sizes are `#define`s in `TemperatureController.h`.
- `DisplayController` — draws to the TFT via TFT_eSPI; takes a `TemperatureController&` and is refreshed from `loop()` with `update(temp, ip, wifiConnected)`.
- `WebServerManager` — ESPAsyncWebServer on port 80; static files from LittleFS plus JSON endpoints `/set_temp` (GET), `/status`, `/history`.

`loop()` flow: periodic WiFi reconnect check → every `TEMP_READ_INTERVAL` read sensor, `controlRelays()`, update display, log to serial → `ArduinoOTA.handle()` → `webServer.handleClients()`. WiFi connect/disconnect arrive via `WiFi.onEvent` callbacks that flip `wifiConnected` and notify the web server. After connecting, setup starts mDNS (`fermentation.local`) and ArduinoOTA.

## Gotchas

- **Relays are active-low**: `LOW` energizes, `HIGH` is off (initialised to `HIGH` in `begin()`).
- **TFT config is in `platformio.ini` build flags** (`-DUSER_SETUP_LOADED`, ST7789 170x320, pins), not `src/User_Setup.h`. Change display pins/driver there. The README's ST7735/128x160 and TFT pin table are outdated.
- **README pin table is also stale for relays**: code uses DS18B20=GPIO21, cooling=22, heating=27 (`TemperatureController.h` is the source of truth).
- `TEMP_READ_INTERVAL` in `main.cpp` is 2000 ms although its comment says "1 minute"; history sizing assumes 60 s sampling (`HISTORY_COUNT` = 24*60), so check `recordTemperature`'s own throttling before changing either.
- WiFi SSID/password are hardcoded in `src/main.cpp` (and currently committed to the working tree); the AP fallback is commented out.
- `build_type = debug` and `CORE_DEBUG_LEVEL=3` are set.
