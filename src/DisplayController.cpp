//
// DisplayController.cpp
// Gauge-style UI for ST7789 170x320 display (portrait)
//
// Drop-in replacement: same constructor, public API and private method
// signatures as before, so DisplayController.h stays unchanged.
// Requires TFT_eSPI >= 2.4 (tft.drawArc).
//
// Layout (170 x 320):
//   0-28    header bar   : title + WiFi status
//   40-210  arc gauge    : current temp, arc fills relative to target (+/-5 C)
//   214-250 target card  : TARGET label + big amber value
//   260-286 status badge : COOLING / HEATING / IDLE (one at a time)
//   296-314 footer       : IP address
//
// The static layout is drawn once; afterwards only the changing parts are
// redrawn, so there is no full-screen flicker.
//

#include "DisplayController.h"

// ---- Local palette / layout (prefixed to avoid clashes with the header) ----
static const uint16_t UI_BG      = 0x0884;  // #0B1220 deep navy
static const uint16_t UI_TRACK   = 0x1947;  // #1E293B arc track / dividers
static const int      UI_CX      = 85;
static const int      UI_CY      = 125;
static const int      UI_R_OUT   = 78;
static const int      UI_R_IN    = 67;
static const int      UI_ARC_A0  = 30;       // arc start angle (0 = 6 o'clock)
static const int      UI_ARC_A1  = 330;      // arc end angle
static const float    UI_RANGE_C = 5.0f;     // gauge spans target +/- this

// True once the static layout of the main screen is on the display.
// Any other full-screen view (boot / error) clears it.
static bool uiLayoutDrawn = false;

// Constructor
DisplayController::DisplayController(TemperatureController& tempController)
    : tempController(tempController),
      lastCurrentTemp(-999.0f),
      lastTargetTemp(-999.0f),
      lastCooling(false),
      lastHeating(false),
      lastWifiConnected(false),
      lastWifiIP{0},
      lastUpdateTime(0)
{
}

void DisplayController::begin() {
    tft.init();
    tft.setRotation(0);  // Portrait orientation (170x320)
    tft.setSwapBytes(true);
    showBootScreen();
}

// ============================================================================
// Boot / error screens
// ============================================================================
void DisplayController::showBootScreen() {
    uiLayoutDrawn = false;
    drawGradientBackground();

    tft.setTextDatum(MC_DATUM);
    tft.setTextPadding(0);

    // Ring logo
    tft.drawArc(DISPLAY_WIDTH / 2, 90, 30, 24, UI_ARC_A0, UI_ARC_A1, C_CURRENT_GREEN, UI_BG, false);
    tft.fillCircle(DISPLAY_WIDTH / 2, 90, 6, C_TARGET_AMBER);

    tft.setTextColor(C_TEXT_PRIMARY);
    tft.setTextSize(2);
    tft.drawString("FERMENTATION", DISPLAY_WIDTH / 2, 160);

    tft.setTextColor(C_TEXT_SECONDARY);
    tft.setTextSize(1);
    tft.drawString("Temperature Controller", DISPLAY_WIDTH / 2, 185);

    // Progress bar
    int barX = 20, barY = 220, barW = 130, barH = 4;
    tft.fillRoundRect(barX, barY, barW, barH, barH / 2, UI_TRACK);
    tft.fillRoundRect(barX, barY, (int)(barW * 0.33f), barH, barH / 2, C_CURRENT_GREEN);

    tft.setTextColor(C_TEXT_MUTED);
    tft.drawString("ESP32 - ST7789 - DS18B20", DISPLAY_WIDTH / 2, 295);

    delay(1500);
}

void DisplayController::showWiFiError() {
    uiLayoutDrawn = false;
    drawGradientBackground();

    tft.setTextDatum(MC_DATUM);
    tft.setTextPadding(0);

    // Warning triangle
    int cx = DISPLAY_WIDTH / 2;
    tft.fillTriangle(cx, 40, cx - 24, 84, cx + 24, 84, C_TARGET_AMBER);
    tft.setTextColor(UI_BG);
    tft.setTextSize(3);
    tft.drawString("!", cx, 68);

    tft.setTextSize(1);
    tft.setTextColor(C_TEXT_PRIMARY);
    tft.drawString("WiFi Unavailable", cx, 115);
    tft.setTextColor(C_TEXT_SECONDARY);
    tft.drawString("Temp monitoring active", cx, 135);

    tft.setTextColor(C_TEXT_MUTED);
    tft.drawString("Running in offline mode", cx, 200);
}

// ============================================================================
// Update loop
// ============================================================================
void DisplayController::update(float currentTemp, const char* wifiIP, bool wifiConnected) {
    unsigned long now = millis();

    // Update every 1 second
    if (now - lastUpdateTime < 1000) {
        return;
    }
    lastUpdateTime = now;

    // Check if anything changed
    bool changed = !uiLayoutDrawn;
    if (currentTemp != lastCurrentTemp) changed = true;
    if (tempController.getTargetTemperature() != lastTargetTemp) changed = true;
    if (tempController.isCooling() != lastCooling) changed = true;
    if (tempController.isHeating() != lastHeating) changed = true;
    if (wifiConnected != lastWifiConnected) changed = true;
    if (wifiConnected && wifiIP && strcmp(wifiIP, lastWifiIP) != 0) changed = true;

    if (!changed) {
        return;
    }

    // Store new values
    lastCurrentTemp = currentTemp;
    lastTargetTemp  = tempController.getTargetTemperature();
    lastCooling     = tempController.isCooling();
    lastHeating     = tempController.isHeating();
    lastWifiConnected = wifiConnected;
    if (wifiConnected && wifiIP) {
        strncpy(lastWifiIP, wifiIP, 16);
        lastWifiIP[16] = '\0';
    }

    drawMainScreen(currentTemp, wifiIP, wifiConnected);
}

// ============================================================================
// Main screen
// ============================================================================
void DisplayController::drawMainScreen(float currentTemp, const char* wifiIP, bool wifiConnected) {
    // One-time static layout
    if (!uiLayoutDrawn) {
        tft.fillScreen(UI_BG);
        tft.drawFastHLine(0, 28, DISPLAY_WIDTH, UI_TRACK);

        // Target card frame + label
        tft.fillRoundRect(10, 214, DISPLAY_WIDTH - 20, 36, 8, C_CARD_BG);
        tft.drawRoundRect(10, 214, DISPLAY_WIDTH - 20, 36, 8, C_CARD_BORDER);
        tft.setTextDatum(ML_DATUM);
        tft.setTextPadding(0);
        tft.setTextColor(C_TEXT_MUTED, C_CARD_BG);
        tft.setTextSize(1);
        tft.drawString("TARGET", 18, 232);

        // Target marker at the top of the gauge (target sits at 12 o'clock)
        tft.fillTriangle(UI_CX - 5, UI_CY - UI_R_OUT - 10,
                         UI_CX + 5, UI_CY - UI_R_OUT - 10,
                         UI_CX,     UI_CY - UI_R_OUT - 2, C_TARGET_AMBER);

        drawTitle();
        uiLayoutDrawn = true;
    }

    // Colour by distance from target
    uint16_t tempColor;
    float diff = currentTemp - lastTargetTemp;
    if (currentTemp < -50) {
        tempColor = C_CURRENT_RED;      // Sensor error
    } else if (diff > 1.0f) {
        tempColor = C_CURRENT_RED;      // Too hot
    } else if (diff < -1.0f) {
        tempColor = C_CURRENT_BLUE;     // Too cold
    } else {
        tempColor = C_CURRENT_GREEN;    // On target
    }

    drawCurrentTemp(currentTemp, tempColor);
    drawTargetTemp(lastTargetTemp);
    drawStatusDots(lastCooling, lastHeating);
    drawIPDisplay(wifiIP, wifiConnected);
}

// ============================================================================
// Drawing helpers
// ============================================================================

// Dark vertical gradient (used for boot / error screens only)
void DisplayController::drawGradientBackground() {
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        uint8_t r = 8  + (y * 6  / DISPLAY_HEIGHT);
        uint8_t g = 14 + (y * 14 / DISPLAY_HEIGHT);
        uint8_t b = 28 + (y * 16 / DISPLAY_HEIGHT);
        uint16_t color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        tft.drawFastHLine(0, y, DISPLAY_WIDTH, color);
    }
}

// Header bar: title on the left, WiFi dot drawn by drawIPDisplay on the right
void DisplayController::drawTitle() {
    tft.setTextDatum(ML_DATUM);
    tft.setTextPadding(0);
    tft.setTextColor(C_TEXT_PRIMARY, UI_BG);
    tft.setTextSize(1);
    tft.drawString("FERMENTATION", 12, 14);

    // Small accent dot before the title
    tft.fillCircle(6, 14, 2, C_CURRENT_GREEN);
}

// Arc gauge + big temperature readout
void DisplayController::drawCurrentTemp(float temp, uint16_t color) {
    const int cx = UI_CX, cy = UI_CY;
    bool error = (temp < -50);

    // --- Arc: fills from start; target is at the middle of the sweep ---
    float frac = 0.0f;
    if (!error) {
        float lo = lastTargetTemp - UI_RANGE_C;
        frac = (temp - lo) / (2.0f * UI_RANGE_C);
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
    }
    int valueEnd = UI_ARC_A0 + (int)(frac * (UI_ARC_A1 - UI_ARC_A0));

    if (valueEnd > UI_ARC_A0) {
        tft.drawArc(cx, cy, UI_R_OUT, UI_R_IN, UI_ARC_A0, valueEnd, color, UI_BG, false);
    }
    if (valueEnd < UI_ARC_A1) {
        tft.drawArc(cx, cy, UI_R_OUT, UI_R_IN, valueEnd, UI_ARC_A1,
                    error ? C_CURRENT_RED : UI_TRACK, UI_BG, false);
    }

    // --- Centre readout (padding overwrites the previous value) ---
    tft.setTextDatum(MC_DATUM);
    tft.setTextPadding(110);
    tft.setTextSize(4);

    if (error) {
        tft.setTextColor(C_CURRENT_RED, UI_BG);
        tft.drawString("ERR", cx, cy - 6);
        tft.setTextSize(1);
        tft.setTextPadding(100);
        tft.setTextColor(C_TEXT_MUTED, UI_BG);
        tft.drawString("No sensor", cx, cy + 28);
        tft.setTextPadding(60);
        tft.drawString("", cx, cy + 46);
        tft.setTextPadding(0);
        return;
    }

    char tempStr[10];
    snprintf(tempStr, sizeof(tempStr), "%.1f", temp);
    tft.setTextColor(color, UI_BG);
    tft.drawString(tempStr, cx, cy - 6);

    // Unit line: degree ring + C
    tft.setTextPadding(0);
    tft.setTextSize(2);
    tft.setTextColor(C_TEXT_MUTED, UI_BG);
    tft.drawString("C", cx, cy + 28);
    drawDegreeSymbol(cx - 10 - 3, cy + 28 + 6, C_TEXT_MUTED, 1);

    // Delta from target
    char deltaStr[12];
    snprintf(deltaStr, sizeof(deltaStr), "%+.1f", temp - lastTargetTemp);
    tft.setTextSize(1);
    tft.setTextPadding(60);
    tft.setTextColor(color, UI_BG);
    tft.drawString(deltaStr, cx, cy + 50);
    tft.setTextPadding(0);
}

// Target card value (card frame and label are static)
void DisplayController::drawTargetTemp(float temp) {
    char targetStr[12];
    snprintf(targetStr, sizeof(targetStr), "%.1f", temp);

    tft.setTextDatum(MR_DATUM);
    tft.setTextPadding(70);
    tft.setTextSize(2);
    tft.setTextColor(C_TARGET_AMBER, C_CARD_BG);
    tft.drawString(targetStr, DISPLAY_WIDTH - 42, 232);

    tft.setTextPadding(0);
    tft.setTextSize(1);
    tft.drawString("C", DISPLAY_WIDTH - 27, 234);
    drawDegreeSymbol(DISPLAY_WIDTH - 40, 232-3, C_TARGET_AMBER, 1);
}

// Single status badge: COOLING, HEATING or IDLE (never both)
void DisplayController::drawStatusDots(bool cooling, bool heating) {
    const int x = 10, y = 260, h = 26, w = DISPLAY_WIDTH - 20;

    const char* label;
    uint16_t fill, border, textCol, dotCol;

    if (cooling) {              // cooling wins if both flags are ever set
        label   = "COOLING";
        fill    = C_CURRENT_BLUE;
        border  = C_CURRENT_BLUE;
        textCol = UI_BG;
        dotCol  = UI_BG;
    } else if (heating) {
        label   = "HEATING";
        fill    = C_CURRENT_RED;
        border  = C_CURRENT_RED;
        textCol = UI_BG;
        dotCol  = UI_BG;
    } else {
        label   = "IDLE";
        fill    = C_CARD_BG;
        border  = C_CARD_BORDER;
        textCol = C_TEXT_MUTED;
        dotCol  = C_STATUS_IDLE;
    }

    tft.fillRoundRect(x, y, w, h, h / 2, fill);
    tft.drawRoundRect(x, y, w, h, h / 2, border);

    // Dot + label, centred as a group
    tft.setTextSize(1);
    tft.setTextPadding(0);
    int textW = tft.textWidth(label);
    int groupW = 8 + 8 + textW;               // dot (8) + gap (8) + text
    int gx = x + (w - groupW) / 2;
    tft.fillCircle(gx + 4, y + h / 2, 4, dotCol);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(textCol, fill);
    tft.drawString(label, gx + 16, y + h / 2 + 1);
}

// Footer IP + WiFi indicator in the header
void DisplayController::drawIPDisplay(const char* ip, bool connected) {
    // Header WiFi indicator (right side): three bars
    const int bx = DISPLAY_WIDTH - 24, by = 20;
    uint16_t on  = connected ? C_CURRENT_GREEN : C_CURRENT_RED;
    uint16_t off = UI_TRACK;
    tft.fillRect(bx - 2, 6, 22, 18, UI_BG);
    if (connected) {
        tft.fillRect(bx,      by - 4,  4, 4,  on);
        tft.fillRect(bx + 6,  by - 9,  4, 9,  on);
        tft.fillRect(bx + 12, by - 14, 4, 14, on);
    } else {
        tft.fillRect(bx,      by - 4,  4, 4,  off);
        tft.fillRect(bx + 6,  by - 9,  4, 9,  off);
        tft.fillRect(bx + 12, by - 14, 4, 14, off);
        tft.drawLine(bx - 1, by + 1, bx + 17, by - 15, on);
    }

    // Footer text
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextPadding(DISPLAY_WIDTH - 20);
    if (!connected) {
        tft.setTextColor(C_CURRENT_RED, UI_BG);
        tft.drawString("Offline", DISPLAY_WIDTH / 2, 305);
    } else {
        tft.setTextColor(C_TEXT_SECONDARY, UI_BG);
        tft.drawString((ip && ip[0]) ? ip : "...", DISPLAY_WIDTH / 2, 305);
    }
    tft.setTextPadding(0);
}

// Degree symbol as a small ring; (x, y) is its centre
void DisplayController::drawDegreeSymbol(int16_t x, int16_t y, uint16_t color, uint8_t size) {
    int r = (size == 1) ? 2 : (size == 2) ? 3 : 4;
    tft.drawCircle(x, y - r - 2, r, color);
    if (r > 2) tft.drawCircle(x, y - r - 2, r - 1, color);
}

// Float mapping utility (unused but kept for compatibility)
float DisplayController::mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}