//
// WebServer.h
// ESP32 Web Server for configuring target temperature
// Uses LittleFS for serving HTML files
//

#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include "TemperatureController.h"

class WebServerManager {
public:
    WebServerManager(TemperatureController& tempController);
    
    void begin();
    void handleClients();
    
    bool isConnected();
    void setConnected(bool connected);
    
private:
    TemperatureController& tempController;
    AsyncWebServer server{80};
    
    // Web server components
    // Using ESPAsyncWebServer library
    // We'll use function pointers for route handlers
    
    bool wifiConnected;
    
    // History request parameters
    int historyStart;  // Start index for pagination
    int historyCount;  // Number of points to return
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    void handleSetTemperature(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleHistory(AsyncWebServerRequest* request);
};
