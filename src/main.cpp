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
#include <HTTPClient.h>

// Project includes
#include "TemperatureController.h"
#include "DisplayController.h"
#include "WebServer.h"

// ===== WIFI CONFIGURATION =====
// Change these to match your network
const char* WIFI_SSID = "Lummelunda";
const char* WIFI_PASSWORD = "snaggedagge";

// ===== NTFY NOTIFICATION =====
// Topic name acts as the "password" on ntfy.sh - pick something unguessable
const char* NTFY_TOPIC = "fermentation-controller-ggf12Lmn1";
const float NTFY_TARGET_WINDOW = 1.0f;             // notify when within this many °C of target
const unsigned long NTFY_RETRY_INTERVAL = 30000;   // retry delay after a failed send
bool targetReachedNotified = false;
unsigned long lastNtfyAttempt = 0;

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

// Send a plain-text notification via ntfy.sh. Returns true on success.
bool sendNtfy(const char* title, const String& message) {
    HTTPClient http;
    http.setTimeout(3000);
    String url = String("http://ntfy.sh/") + NTFY_TOPIC;
    if (!http.begin(url)) return false;
    http.addHeader("Title", title);
    http.addHeader("Tags", "beer");
    int code = http.POST(message);
    http.end();
    Serial.printf("ntfy response: %d\n", code);
    return code == 200;
}

// Notify once per boot the first time the temperature is within range of target
void checkTargetReached(float currentTemp) {
    if (targetReachedNotified || !wifiConnected) return;
    float target = tempController.getTargetTemperature();
    if (fabsf(currentTemp - target) > NTFY_TARGET_WINDOW) return;  // also skips sensor error (-999)

    unsigned long now = millis();
    if (lastNtfyAttempt != 0 && now - lastNtfyAttempt < NTFY_RETRY_INTERVAL) return;
    lastNtfyAttempt = now;

    String msg = String("Temperature is ") + String(currentTemp, 1) + " C (target " + String(target, 1) + " C)";
    if (sendNtfy("Fermentation: target reached", msg)) {
        targetReachedNotified = true;
    }
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
        checkTargetReached(currentTemp);
        
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
