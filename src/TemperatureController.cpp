//
// TemperatureController.cpp
// Implementation of temperature reading and relay control
//

#include "TemperatureController.h"

TemperatureController::TemperatureController()
    : oneWire(nullptr), sensor(nullptr),
      targetTemperature(DEFAULT_TARGET_TEMP),
      coolingActive(false), heatingActive(false),
      lastReadTime(0), readInterval(2000),
      historyIndex(0), historyCount(0), currentTemperature(0.0f), lastHistoryRecordTime(0)
{
    // Initialize history buffer
    memset(history, 0, sizeof(history));
    memset(historyTimestamps, 0, sizeof(historyTimestamps));
    memset(historyStates, 0, sizeof(historyStates));
}

void TemperatureController::begin() {
    // Setup relay pins
    pinMode(COOLING_RELAY_PIN, OUTPUT);
    pinMode(HEATING_RELAY_PIN, OUTPUT);
    digitalWrite(COOLING_RELAY_PIN, HIGH);
    digitalWrite(HEATING_RELAY_PIN, HIGH);
    
    // Setup DS18B20 sensor
    oneWire = new OneWire(DS18B20_PIN);
    sensor = new DallasTemperature(oneWire);
    sensor->begin();
    
    // Find all devices on the bus
    sensor->begin();
    if (sensor->getDeviceCount() == 0) {
        Serial.println("WARNING: No DS18B20 sensors found!");
    } else {
        int deviceCount = sensor->getDeviceCount();
        Serial.printf("Found %d DS18B20 sensor(s)\n", deviceCount);
        
        // Set resolution to 12-bit (default, but be explicit)
        for (int i = 0; i < deviceCount; i++) {
            DeviceAddress addr;
            sensor->getAddress(addr, i);
            sensor->setResolution(addr, 12);
        }
    }
    
    // Load saved target temperature from NVM
    loadTargetTemperature();
    
    Serial.printf("Target temperature: %.1f°C\n", targetTemperature);
}

float TemperatureController::readTemperature(bool readFromSensor) {
    unsigned long now = millis();
    if (!readFromSensor || now - lastReadTime < readInterval) {
        return currentTemperature;
    }
    
    lastReadTime = now;
    
    sensor->requestTemperatures();
    
    // Wait for conversion (max 750ms for 12-bit)
    delay(750);
    
    float temp = sensor->getTempCByIndex(0);
    
    // Check for errors
    if (temp == DEVICE_DISCONNECTED_C) {
        temp = -999.0f;
        Serial.println("ERROR: DS18B20 disconnected");
    }
    currentTemperature = temp;
    
    if (now - lastHistoryRecordTime >= 60000) {
        recordTemperature(temp);
        lastHistoryRecordTime = now;
    }
    
    return temp;
}

void TemperatureController::recordTemperature(float temp) {
    uint32_t now = millis();
    
    // Handle millis rollover
    static uint32_t lastMillis = 0;
    if (now < lastMillis) {
        // Rollover - reset history
        historyCount = 0;
        historyIndex = 0;
    }
    lastMillis = now;
    
    // Store in ring buffer
    history[historyIndex] = temp;
    historyTimestamps[historyIndex] = now/1000;
    historyStates[historyIndex] = heatingActive ? 2 : (coolingActive ? 1 : 0);
    historyIndex = (historyIndex + 1) % HISTORY_COUNT;
    
    if (historyCount < HISTORY_COUNT) {
        historyCount++;
    }
}

int TemperatureController::getHistory(float* temps, uint32_t timestamps[], uint8_t* states, int maxPoints) {
    if (historyCount == 0) {
        return 0;
    }
    
    // Calculate how many points we have
    int totalPoints = historyCount;
    
    // If we have fewer points than requested, return all of them
    if (totalPoints <= maxPoints) {
        int count = 0;
        for (int i = 0; i < totalPoints; i++) {
            int idx = (historyIndex - totalPoints + i + HISTORY_COUNT) % HISTORY_COUNT;
            temps[count] = history[idx];
            timestamps[count] = historyTimestamps[idx];
            states[count] = historyStates[idx];
            count++;
        }
        return count;
    }
    
    // Downsample: pick evenly spaced points from the oldest data
    // We want the last maxPoints points spread across the available data
    int count = 0;
    int step = (float)totalPoints / maxPoints;
    
    for (int i = 0; i < maxPoints; i++) {
        int srcIdx = (historyIndex - totalPoints + i * step + HISTORY_COUNT) % HISTORY_COUNT;
        temps[count] = history[srcIdx];
        timestamps[count] = historyTimestamps[srcIdx];
        states[count] = historyStates[srcIdx];
        count++;
    }
    
    return count;
}

void TemperatureController::controlRelays(float currentTemp) {
    if (currentTemp < 0) {
        // Sensor error, turn off both relays
        digitalWrite(COOLING_RELAY_PIN, HIGH);
        coolingActive = false;
        digitalWrite(HEATING_RELAY_PIN, HIGH);
        heatingActive = false;
        Serial.println("ERROR: Turning off relays due to sensor error");
        return;
    }
    
    // Hysteresis control
    // Cooling turns on when temp > target + hysteresis/2
    // Cooling turns off when temp <= target
    // Heating turns on when temp < target - hysteresis/2
    // Heating turns off when temp >= target
    
    float coolThreshold = targetTemperature + TEMPERATURE_HYSTERESIS / 2.0f;
    float heatThreshold = targetTemperature - TEMPERATURE_HYSTERESIS / 2.0f;
    
    // Cooling logic
    if (currentTemp > coolThreshold) {
        digitalWrite(COOLING_RELAY_PIN, LOW);
        coolingActive = true;
        Serial.println("Turning on cooling relay");
    } 
    else if (currentTemp <= targetTemperature) {
        digitalWrite(COOLING_RELAY_PIN, HIGH);
        coolingActive = false;
        Serial.println("Turning off cooling relay");
    }
    
    // Heating logic
    if (currentTemp < heatThreshold) {
        digitalWrite(HEATING_RELAY_PIN, LOW);
        heatingActive = true;
        Serial.println("Turning on heating relay");
    } else if (currentTemp >= targetTemperature) {
        digitalWrite(HEATING_RELAY_PIN, HIGH);
        heatingActive = false;
        Serial.println("Turning off heating relay");
    }
}

float TemperatureController::getTargetTemperature() {
    return targetTemperature;
}

void TemperatureController::setTargetTemperature(float temp) {
    // Clamp to valid range
    if (temp < MIN_TARGET_TEMP) temp = MIN_TARGET_TEMP;
    if (temp > MAX_TARGET_TEMP) temp = MAX_TARGET_TEMP;
    
    if (temp != targetTemperature) {
        targetTemperature = temp;
        Serial.printf("Target temperature changed to: %.1f°C\n", targetTemperature);
    }
}

void TemperatureController::saveTargetTemperature() {
    preferences.begin(NVM_NAMESPACE, false);  // Write mode
    preferences.putFloat(NVM_KEY_TARGET_TEMP, targetTemperature);
    preferences.end();
    Serial.println("Target temperature saved to NVM");
}

void TemperatureController::loadTargetTemperature() {
    preferences.begin(NVM_NAMESPACE, true);  // Read mode
    targetTemperature = preferences.getFloat(NVM_KEY_TARGET_TEMP, DEFAULT_TARGET_TEMP);
    preferences.end();
    
    // Validate loaded value
    if (targetTemperature < MIN_TARGET_TEMP || targetTemperature > MAX_TARGET_TEMP) {
        targetTemperature = DEFAULT_TARGET_TEMP;
    }
}

bool TemperatureController::isCooling() {
    return coolingActive;
}

bool TemperatureController::isHeating() {
    return heatingActive;
}

// Get current time in seconds since epoch (approximate)
static uint32_t getCurrentEpoch() {
    // Use millis() as a relative timestamp
    // For proper epoch time, you'd sync with NTP when WiFi is available
    return millis() / 1000;
}
