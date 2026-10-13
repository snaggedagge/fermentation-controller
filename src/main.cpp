//
// Fermentation Temperature Controller
// Main entry point - ESP32 with ST7789 LCD, DS18B20 temp sensor, and WiFi web server
//
// Hardware connections: See TemperatureController.h
//
// Features:
//   - WiFi AP/STA mode with web interface for temperature control
//   - Real-time temperature display on LCD
//   - Hysteresis-based relay control for heating/cooling
//   - Persistent target temperature storage
//

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

// Project includes
#include "TemperatureController.h"
#include "DisplayController.h"
#include "WebServer.h"

// ===== WIFI CONFIGURATION =====
// Change these to match your network
const char* WIFI_SSID = "4G-Gateway-21E0";
const char* WIFI_PASSWORD = "snaggedagge";

// ===== GLOBAL INSTANCES =====
TemperatureController tempController;
DisplayController displayController(tempController);
WebServerManager webServer(tempController);

// ===== STATE TRACKING =====
bool wifiConnected = false;
unsigned long lastTempRead = 0;
unsigned long lastWifiCheck = 0;
const unsigned long TEMP_READ_INTERVAL = 2000;   // Read temp every 1 minute
const unsigned long WIFI_CHECK_INTERVAL = 10000;   // Check WiFi every 10 seconds
char wifiIPBuffer[17];  // Persistent buffer for WiFi IP string

// ===== WIFI CALLBACKS =====
void onWiFiConnect(const WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("Connected to WiFi");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    wifiConnected = true;
    webServer.setConnected(true);
}

void onWiFiDisconnect(const WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("Disconnected from WiFi");
    wifiConnected = false;
    webServer.setConnected(false);
}

// ===== SETUP =====
void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(100);
    Serial.println("\n========================================");
    Serial.println("  FERMENTATION TEMPERATURE CONTROLLER");
    Serial.println("========================================");
    
    // Initialize temperature controller (relays + DS18B20)
    tempController.begin();
    
    // Initialize display
    displayController.begin();
    
    // Initialize WiFi
    Serial.println("Initializing WiFi...");
    
    // Set up WiFi event handlers
    WiFi.onEvent(onWiFiConnect, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(onWiFiDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    
    // Connect to WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    
    // Wait for WiFi connection (with timeout)
    int connectAttempts = 0;
    const int maxAttempts = 30;  // 30 seconds max
    
    while (connectAttempts < maxAttempts && !wifiConnected) {
        delay(1000);
        connectAttempts++;
        Serial.print(".");
    }
    
    if (wifiConnected) {
        Serial.println("\nWiFi connected!");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        
        // Start mDNS
        if (MDNS.begin("fermentation")) {
            Serial.println("mDNS responder started");
            MDNS.addService("http", "tcp", 80);
            Serial.println("HTTP service added to mDNS");
        }
        
        // Configure and start OTA
        ArduinoOTA
            .setHostname("fermentation-controller")
            .onStart([]() {
                Serial.println("OTA update started");
            })
            .onEnd([]() {
                Serial.println("OTA update finished");
            })
            .onError([](ota_error_t error) {
                Serial.printf("OTA error [%d]: ", error);
                if (error == OTA_AUTH_ERROR) Serial.println("Auth failed");
                else if (error == OTA_BEGIN_ERROR) Serial.println("Begin failed");
                else if (error == OTA_CONNECT_ERROR) Serial.println("Connect failed");
                else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive failed");
                else if (error == OTA_END_ERROR) Serial.println("End failed");
            });
        ArduinoOTA.begin();
    } else {
        Serial.println("\nWiFi connection timed out!");
        //Serial.println("You can still access the web server in AP mode...");
        
        // Optional: Start AP mode as fallback
        //WiFi.softAP("Fermentation-Controller", "snaggedagge");
        //Serial.println("AP mode started, IP: 192.168.4.1");
    }
    
    // Initialize web server
    webServer.begin();
    
    Serial.println("System initialized successfully!");
    Serial.println("========================================\n");
    delay(1000);
}

// ===== MAIN LOOP =====
void loop() {
    // Handle WiFi reconnection if needed
    unsigned long now = millis();
    
    if (!wifiConnected && (now - lastWifiCheck > WIFI_CHECK_INTERVAL)) {
        lastWifiCheck = now;
        
        if (WiFi.status() == WL_CONNECTED) {
            onWiFiConnect(ARDUINO_EVENT_WIFI_STA_CONNECTED, WiFiEventInfo_t{});
        } else {
            // Try to reconnect
            WiFi.reconnect();
        }
    }
    
    // Read temperature at intervals
    if (now - lastTempRead > TEMP_READ_INTERVAL) {
        lastTempRead = now;
        
        float currentTemp = tempController.readTemperature();
        // Control relays based on temperatureThe 
        tempController.controlRelays(currentTemp);
        
        // Update display
        strncpy(wifiIPBuffer, wifiConnected ? WiFi.localIP().toString().c_str() : "", 16);
        wifiIPBuffer[16] = '\0';
        displayController.update(currentTemp, wifiIPBuffer, wifiConnected);
        
        // Print to serial for debugging
        Serial.printf("Temp: %.1f°C | Target: %.1f°C | Cooling: %s | Heating: %s\n",
            currentTemp,
            tempController.getTargetTemperature(),
            tempController.isCooling() ? "ON" : "OFF",
            tempController.isHeating() ? "ON" : "OFF");
    }
    
    // Handle OTA updates (non-blocking)
    ArduinoOTA.handle();
    
    // Handle web server (non-blocking)
    webServer.handleClients();
    
    // Small delay to prevent watchdog issues
    delay(10);
}
