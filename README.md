# Fermentation Temperature Controller

ESP32-based fermentation temperature controller with WiFi web interface and LCD display.

## Hardware

- **MCU**: SparkFun ESP32 Board (or compatible ESP32 DevKit)
- **Display**: 1.9" ST7735 SPI LCD (128x160)
- **Temperature Sensor**: DS18B20 (waterproof probe recommended)
- **Relays**: 2x relay modules (cooling + heating)

## Pin Connections

| Component | Pin | GPIO |
|-----------|-----|------|
| DS18B20 Data | Data | GPIO 21 |
| Cooling Relay | IN | GPIO 22 |
| Heating Relay | IN | GPIO 23 |
| LCD CS | CS | GPIO 5 |
| LCD DC | D/C | GPIO 16 |
| LCD RST | RST | GPIO 4 |
| LCD MOSI | MOSI | GPIO 19 |
| LCD SCLK | SCLK | GPIO 18 |

## Features

- Real-time temperature monitoring with DS18B20
- Hysteresis-based heating/cooling control
- WiFi web interface for remote temperature setting
- Persistent target temperature storage ( survives power cycles)
- Color-coded LCD display showing current/target temperature
- Relay status indicators on both LCD and web interface

## Setup

1. Edit `src/main.cpp` and update WiFi credentials:
   ```cpp
   const char* WIFI_SSID = "your_wifi_ssid";
   const char* WIFI_PASSWORD = "your_wifi_password";
   ```

2. Update board type in `platformio.ini` if using a specific SparkFun board:
   ```ini
   board = sparkfun_esp32_thing_plus
   ```

3. Build and upload:
   ```bash
   pio run --target upload
   ```

4. Find the device IP address from serial monitor or router

5. Open a browser and navigate to the IP address

## Web Interface

Access the web interface at `http://<device-ip>` to:
- View current and target temperature
- Adjust target temperature with +/- buttons
- See cooling/heating status

## LCD Display

The LCD shows:
- Current temperature (large, color-coded)
- Target temperature
- Cooling/Heating status indicators
- WiFi IP address
- System status

## Temperature Range

- **Minimum**: 0°C (41°F)
- **Maximum**: 30°C (86°F)
- **Default**: 20°C (68°F)
- **Hysteresis**: 0.3°C (prevents relay chatter)

## Dependencies

- TFT_eSPI - LCD display library
- OneWire - DS18B20 communication
- DallasTemperature - DS18B20 driver
- ESPAsyncWebServer - Web server
- ArduinoJson - JSON handling

## Building

```bash
# Install PlatformIO if not already installed
pip install platformio

# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

## License

MIT License
