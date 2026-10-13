//
// TemperatureController.h
// Manages temperature readings, PID-like control logic, and relay outputs
//

#pragma once

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

// Hardware pin definitions
#define DS18B20_PIN 21       // DS18B20 data pin
#define COOLING_RELAY_PIN 22 // Cooling relay
#define HEATING_RELAY_PIN 27 // Heating relay

// Hysteresis for relay control (prevents rapid switching)
#define TEMPERATURE_HYSTERESIS 0.5f  // °C

// Target temperature range
#define MIN_TARGET_TEMP 0.0f   // °C
#define MAX_TARGET_TEMP 30.0f  // °C

// Default target temperature
#define DEFAULT_TARGET_TEMP 20.0f

// Temperature history
#define HISTORY_COUNT 1440     // 24 hours at 60000ms intervals (24*60)
#define HISTORY_POINTS 360     // Points returned to web (downsampled)

// NVM storage key
#define NVM_NAMESPACE "temp_ctrl"
#define NVM_KEY_TARGET_TEMP "target_temp"

class TemperatureController {
public:
    TemperatureController();
    
    void begin();
    float readTemperature(bool readFromSensor = true);
    void controlRelays(float currentTemp);
    float getTargetTemperature();
    void setTargetTemperature(float temp);
    void saveTargetTemperature();
    void loadTargetTemperature();
    bool isCooling();
    bool isHeating();
    
    // Temperature history
    void recordTemperature(float temp);
    int getHistory(float* temps, uint32_t timestamps[], uint8_t* states, int maxPoints);

private:
    OneWire* oneWire;
    DallasTemperature* sensor;
    Preferences preferences;
    
    float targetTemperature;
    float currentTemperature;
    bool coolingActive;
    bool heatingActive;
    
    unsigned long lastReadTime;
    unsigned long lastHistoryRecordTime;
    unsigned long readInterval;  // ms between temperature reads
    
    // Temperature history ring buffer
    float history[HISTORY_COUNT];
    uint32_t historyTimestamps[HISTORY_COUNT];
    uint8_t historyStates[HISTORY_COUNT];   // 0 = idle, 1 = cooling, 2 = heating
    int historyIndex;  // Next write position
    int historyCount;  // Number of valid entries
};
