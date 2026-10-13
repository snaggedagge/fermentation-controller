 //
 // WebServer.cpp
 // Implementation of the web server for temperature control
 // Uses ESPAsyncWebServer v3.x API with LittleFS
 //

 #include "WebServer.h"

 #include <ArduinoJson.h>

 // Static instance pointer for callback functions
 static WebServerManager* g_webServer = nullptr;

 WebServerManager::WebServerManager(TemperatureController& tempController)
     : tempController(tempController), wifiConnected(false), server(80)
 {
     g_webServer = this;
 }

 void WebServerManager::begin() {
     // Mount LittleFS
     if (!LittleFS.begin(true)) {
         Serial.println("LittleFS mount failed");
         return;
     }
     Serial.println("LittleFS mounted successfully");

     // Serve static file from LittleFS
     server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

     // Set temperature endpoint
     server.on("/set_temp", HTTP_GET, [](AsyncWebServerRequest* request) {
         g_webServer->handleSetTemperature(request);
     });

     // Status API (JSON)
     server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
         g_webServer->handleStatus(request);
     });

     // Temperature history API (JSON)
     server.on("/history", HTTP_GET, [](AsyncWebServerRequest* request) {
         g_webServer->handleHistory(request);
     });

     // Handle not found
     server.onNotFound([](AsyncWebServerRequest* request) {
         g_webServer->handleNotFound(request);
     });

     server.begin();
     Serial.println("Web server started");
 }

 void WebServerManager::handleClients() {
     // ESPAsyncWebServer v3.x handles clients internally
 }

 bool WebServerManager::isConnected() {
     return wifiConnected;
 }

 void WebServerManager::setConnected(bool connected) {
     wifiConnected = connected;
 }

 // Root page is now handled by LittleFS static file serving
 // No need for handleRoot - the static file server handles it

 void WebServerManager::handleSetTemperature(AsyncWebServerRequest* request) {
     if (!request->hasArg("temp")) {
         request->send(400, "text/plain", "Missing temperature parameter");
         return;
     }

     float temp = request->arg("temp").toFloat();

     // Validate range
     if (temp < MIN_TARGET_TEMP || temp > MAX_TARGET_TEMP) {
         request->send(400, "text/plain",
             String("Temperature must be between ") + MIN_TARGET_TEMP + " and " + MAX_TARGET_TEMP + "°C");
         return;
     }

     tempController.setTargetTemperature(temp);
     tempController.saveTargetTemperature();

     request->send(200, "text/plain", "Temperature set to " + String(temp, 1) + "°C");
 }

 void WebServerManager::handleStatus(AsyncWebServerRequest* request) {
     JsonDocument doc;
     doc["current_temp"] = tempController.readTemperature(false);
     doc["target_temp"] = tempController.getTargetTemperature();
     doc["cooling_active"] = tempController.isCooling();
     doc["heating_active"] = tempController.isHeating();
     doc["wifi_connected"] = wifiConnected;

     String json;
     serializeJson(doc, json);

     request->send(200, "application/json", json);
 }

 void WebServerManager::handleNotFound(AsyncWebServerRequest* request) {
     request->send(404, "text/plain", "Not Found");
 }

void WebServerManager::handleHistory(AsyncWebServerRequest* request) {
    int hours = 1;
    if (request->hasArg("hours")) {
        hours = constrain(request->arg("hours").toInt(), 1, 24);
    }

    // Statiskt = inte på async_tcp-stacken (handlers körs sekventiellt i samma task)
    static float temps[HISTORY_COUNT];
    static uint32_t timestamps[HISTORY_COUNT];
    static uint8_t states[HISTORY_COUNT];
    int total = tempController.getHistory(temps, timestamps, states, HISTORY_COUNT);

    const int MAX_POINTS = 240;
    int window = min(hours * 60, total);   // senaste 'window' punkterna
    int start  = total - window;
    int out    = min(window, MAX_POINTS);

    JsonDocument doc;
    JsonArray arr = doc["data"].to<JsonArray>();

    for (int i = 0; i < out; i++) {
        // Jämnt fördelat över fönstret, inkluderar både första och sista punkten
        int src = (out == 1) ? start + window - 1
                             : start + (long)i * (window - 1) / (out - 1);
        float t = temps[src];
        JsonObject p = arr.add<JsonObject>();
        p["t"] = timestamps[src];
        p["temp"] = isnan(t) ? -127.0f : t;   // frontend filtrerar < -50
        p["s"] = states[src];
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}
