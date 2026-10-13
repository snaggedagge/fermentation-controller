//
// DisplayController.h
// Webpage-inspired UI for 1.9" ST7789 LCD (170x320)
//

#pragma once

#include <TFT_eSPI.h>
#include "TemperatureController.h"

// Display dimensions (1.9" ST7789)
#define DISPLAY_WIDTH 170
#define DISPLAY_HEIGHT 320

// Webpage-inspired color palette (matches the dark blue theme)
#define C_BG_TOP          ((uint16_t)0x0C2340)  // Dark navy
#define C_BG              ((uint16_t)0x0A1A30)  // Slightly lighter navy
#define C_TEXT_PRIMARY    ((uint16_t)0xFFFFFF)
#define C_TEXT_SECONDARY  ((uint16_t)0x6BA3D6)
#define C_TEXT_MUTED      ((uint16_t)0x888888)
#define C_CURRENT_GREEN   ((uint16_t)0x00E676)
#define C_CURRENT_RED     ((uint16_t)0xFF5252)
#define C_CURRENT_BLUE    ((uint16_t)0x448AFF)
#define C_TARGET_AMBER    ((uint16_t)0xFFD740)
#define C_STATUS_IDLE     ((uint16_t)0x666666)
#define C_CARD_BG         ((uint16_t)0x1A2A40)
#define C_CARD_BORDER     ((uint16_t)0x2A3A50)

class DisplayController {
public:
    DisplayController(TemperatureController& tempController);
    
    void begin();
    void update(float currentTemp, const char* wifiIP, bool wifiConnected);
    void showBootScreen();
    void showWiFiError();
    
    // Getters for current state
    bool isCooling() { return tempController.isCooling(); }
    bool isHeating() { return tempController.isHeating(); }
    
private:
    TemperatureController& tempController;
    TFT_eSPI tft;
    
    // State tracking
    float lastCurrentTemp;
    float lastTargetTemp;
    bool lastCooling;
    bool lastHeating;
    bool lastWifiConnected;
    char lastWifiIP[17];
    unsigned long lastUpdateTime;
    
    // Drawing helpers
    void drawMainScreen(float currentTemp, const char* wifiIP, bool wifiConnected);
    void drawGradientBackground();
    void drawTitle();
    void drawCurrentTemp(float temp, uint16_t color);
    void drawTargetTemp(float temp);
    void drawStatusDots(bool cooling, bool heating);
    void drawIPDisplay(const char* ip, bool connected);
    void drawDegreeSymbol(int16_t x, int16_t y, uint16_t color, uint8_t size = 1);
    
    // Utility
    float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
};
