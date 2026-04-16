#include "Settings.h"
#include <EEPROM.h>

// ---------------------------------------------------------------------------
// Settings.cpp
// Flat binary serialisation to EEPROM emulation.
// Layout:
//   [0-1]  magic key (uint16_t)
//   [2 …]  raw bytes of Settings struct (everything except the methods)
// ---------------------------------------------------------------------------

// Total EEPROM size needed: 2 bytes magic + sizeof(Settings data)
// We define a plain-old-data mirror struct for the data portion to keep the
// serialisation simple.
struct SettingsData {
    uint8_t       operatingMode;
    uint16_t      modeDecisionTimeMin;
    uint8_t       recircStartBedTemp;
    uint8_t       recircStartSpeed;
    bool          chamberLightOn;
    HotChamberSettings  hot;
    ColdChamberSettings cold;
    DebugSettings       debug;
    NetworkSettings     network;
};

static const size_t EEPROM_SIZE = sizeof(uint16_t) + sizeof(SettingsData);

// Global instance
Settings gSettings;

// ---------------------------------------------------------------------------
Settings::Settings() {
    applyDefaults();
}

void Settings::applyDefaults() {
    operatingMode        = OperatingMode::Auto;
    modeDecisionTimeMin  = 10;
    recircStartBedTemp   = 45;
    recircStartSpeed     = 30;

    hot.heatingFanSpeed   = 50;
    hot.recircFanSpeed    = 50;
    hot.exhaustFanSpeed   = 15;
    hot.bedTempThreshold  = 60;

    cold.exhaustFanMax    = 100;
    cold.exhaustFanMin    = 15;
    cold.recircFanSpeed   = 20;
    cold.maxChamberTemp   = 38;
    cold.pidKp            = 2.0f;
    cold.pidKi            = 0.5f;
    cold.pidKd            = 1.0f;

    debug.debugMode        = false;
    debug.debugChamberTemp = 25.0f;
    debug.debugBedTemp     = 25.0f;
    debug.manualFanControl = false;
    debug.manualHeatingFanSpeed = 0;
    debug.manualExhaustFanSpeed = 0;
    debug.manualRecircFanSpeed = 0;
    debug.heatingFanPresent    = true;
    debug.exhaustFanPresent    = true;
    debug.recircFanPresent     = true;
    debug.chamberLightPresent  = true;
    debug.logIntervalMin       = 5;

    chamberLightOn = false;

    network.ssid[0]     = '\0';
    network.password[0] = '\0';
}

bool Settings::load() {
    EEPROM.begin(EEPROM_SIZE);

    uint16_t magic = 0;
    EEPROM.get(0, magic);
    if (magic != SETTINGS_MAGIC) {
        EEPROM.end();
        return false;  // No valid data — caller should save defaults
    }

    SettingsData data;
    EEPROM.get(sizeof(uint16_t), data);
    EEPROM.end();

    operatingMode         = static_cast<OperatingMode>(data.operatingMode);
    modeDecisionTimeMin   = data.modeDecisionTimeMin;
    recircStartBedTemp    = data.recircStartBedTemp;
    recircStartSpeed      = data.recircStartSpeed;
    chamberLightOn        = data.chamberLightOn;
    hot                   = data.hot;
    cold                  = data.cold;
    debug                 = data.debug;
    network               = data.network;

    return true;
}

void Settings::save() {
    EEPROM.begin(EEPROM_SIZE);

    EEPROM.put(0, static_cast<uint16_t>(SETTINGS_MAGIC));

    SettingsData data;
    data.operatingMode        = static_cast<uint8_t>(operatingMode);
    data.modeDecisionTimeMin  = modeDecisionTimeMin;
    data.recircStartBedTemp   = recircStartBedTemp;
    data.recircStartSpeed     = recircStartSpeed;
    data.chamberLightOn       = chamberLightOn;
    data.hot                  = hot;
    data.cold                 = cold;
    data.debug                = debug;
    data.network              = network;

    EEPROM.put(sizeof(uint16_t), data);
    EEPROM.commit();
    EEPROM.end();
}

void Settings::resetToDefaults() {
    applyDefaults();
    save();
}
