#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Settings.h
// All user-configurable settings for FanController2.
// Values are persisted to EEPROM (ESP32 flash emulation) via TcMenu's
// EepromAbstraction. Call Settings::load() once on boot and
// Settings::save() whenever a value changes.
// ---------------------------------------------------------------------------

// Magic key written at the start of EEPROM to detect valid data.
// Change this value if you restructure the EEPROM layout to force a reset.
#define SETTINGS_MAGIC  0xFC02

// EEPROM base address used by settings (TcMenu occupies from this address)
#define SETTINGS_EEPROM_BASE  2   // bytes 0-1 reserved for magic key

struct HotChamberSettings {
    uint8_t  heatingFanSpeed;        // % (default 50)
    uint8_t  recircFanSpeed;         // % (default 50)
    uint8_t  exhaustFanSpeed;        // % (default 15)
    uint8_t  bedTempThreshold;       // °C (default 60)
};

struct ColdChamberSettings {
    uint8_t  exhaustFanMax;          // % (default 100)
    uint8_t  exhaustFanMin;          // % (default 15)
    uint8_t  recircFanSpeed;         // % (default 20)
    uint8_t  maxChamberTemp;         // °C (default 38)
    float    pidKp;                  // (default 2.0)
    float    pidKi;                  // (default 0.5)
    float    pidKd;                  // (default 1.0)
};

struct DebugSettings {
    bool     debugMode;              // false = normal, true = use simulated temps
    float    debugChamberTemp;       // °C simulated chamber temperature
    float    debugBedTemp;           // °C simulated bed temperature
    bool     manualFanControl;       // true = use manual fan override values
    uint8_t  manualHeatingFanSpeed;  // % manual heating fan speed
    uint8_t  manualExhaustFanSpeed;  // % manual exhaust fan speed
    uint8_t  manualRecircFanSpeed;   // % manual recirculation fan speed
    bool     heatingFanPresent;      // false = heating fan not installed
    bool     exhaustFanPresent;      // false = exhaust fan not installed
    bool     recircFanPresent;       // false = recirculation fan not installed
};

// Operating mode selected by the user in the Settings menu.
enum class OperatingMode : uint8_t {
    Auto = 0,
    Heat = 1,
    Cool = 2
};

class Settings {
public:
    // --- Top-level settings -------------------------------------------------
    OperatingMode operatingMode;       // AUTO / HEAT / COOL (default: Auto)
    uint16_t      modeDecisionTimeMin; // minutes (default 10)
    uint8_t       recircStartBedTemp;  // °C (default 45)
    uint8_t       recircStartSpeed;    // % (default 30)

    HotChamberSettings  hot;
    ColdChamberSettings cold;
    DebugSettings       debug;

    // Initialise all fields to firmware defaults.
    Settings();

    // Load from EEPROM. Returns true if a valid magic key was found.
    // If the key is missing or wrong the defaults are kept and true is NOT
    // returned — call save() afterwards to write the defaults.
    bool load();

    // Persist all settings to EEPROM immediately.
    void save();

    // Reset all settings to firmware defaults and save.
    void resetToDefaults();

private:
    void applyDefaults();
};

// Global singleton — included by any file that needs access to settings.
extern Settings gSettings;
