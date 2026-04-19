#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

// ---------------------------------------------------------------------------
// TemperatureSensors.h
// Manages two DS18B20 sensors on separate 1-Wire buses (one per GPIO).
//
// Typical usage (called from a TaskManager periodic task):
//   1. sensors.requestAll()    — start async conversion (non-blocking)
//   2. (wait ~750 ms for 12-bit conversion — handled by task interval)
//   3. sensors.update()        — read and cache the results
//   4. sensors.getChamberTemp() / getBedTemp()  — use the cached values
//
// Returns TEMP_READ_ERROR (-127.0) if a sensor is disconnected or faulty.
// ---------------------------------------------------------------------------

class TemperatureSensors {
public:
    TemperatureSensors();

    // Initialise both 1-Wire buses and configure sensor resolution.
    void begin();

    // Send async conversion command to both buses (non-blocking).
    void requestAll();

    // Read the completed conversion results and cache them internally.
    // Call this at least 750 ms after requestAll().
    void update();

    // Return the most recently cached temperature readings.
    float getChamberTemp() const { return _chamberTemp; }
    float getBedTemp()     const { return _bedTemp; }

    // Return the effective temperature, respecting debug mode.
    // If debug mode is active, returns the simulated value from settings.
    float getEffectiveChamberTemp() const;
    float getEffectiveBedTemp()     const;

    // True if the most recent read returned a valid value.
    bool isChamberOk() const { return _chamberOk; }
    bool isBedOk()     const { return _bedOk; }

private:
    OneWire           _chamberBus;
    OneWire           _bedBus;
    DallasTemperature _chamberSensor;
    DallasTemperature _bedSensor;

    float _chamberTemp;
    float _bedTemp;
    bool  _chamberOk;
    bool  _bedOk;
};
