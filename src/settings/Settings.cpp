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
    uint16_t      lowPinPwmFreqHz;
    HotChamberSettings  hot;
    ColdChamberSettings cold;
    DebugSettings       debug;
    NetworkSettings     network;
};

static const size_t EEPROM_SIZE = sizeof(uint16_t) + sizeof(SettingsData);

// Global instance
Settings gSettings;

const char* fanTypeToString(FanType type) {
    switch (type) {
        case FanType::Pin2: return "2PIN";
        case FanType::Pin3: return "3PIN";
        case FanType::Pin4: return "4PIN";
        default:            return "UNKNOWN";
    }
}

const char* operatingModeToString(OperatingMode mode) {
    switch (mode) {
        case OperatingMode::Auto:   return "AUTO";
        case OperatingMode::Heat:   return "HEATING";
        case OperatingMode::Cool:   return "COOLING";
        case OperatingMode::Manual: return "MANUAL";
        default:                    return "UNKNOWN";
    }
}

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
    debug.heatingFanType       = FanType::Pin4;
    debug.exhaustFanType       = FanType::Pin4;
    debug.recircFanType        = FanType::Pin4;

    chamberLightOn = false;
    lowPinPwmFreqHz = 100;

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
    lowPinPwmFreqHz       = data.lowPinPwmFreqHz;
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
    data.lowPinPwmFreqHz      = lowPinPwmFreqHz;
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

void Settings::dump(Print& out) const {
    out.printf(" mode          : %s\n", operatingModeToString(operatingMode));
    out.printf(" mdt           : %d min\n",  modeDecisionTimeMin);
    out.printf(" rfsbt         : %d C\n",    recircStartBedTemp);
    out.printf(" recirc speed  : %d %%\n",   recircStartSpeed);
    out.println(" -- Hot chamber --");
    out.printf(" heating fan   : %d %%\n",   hot.heatingFanSpeed);
    out.printf(" hot recirc    : %d %%\n",   hot.recircFanSpeed);
    out.printf(" hot exhaust   : %d %%\n",   hot.exhaustFanSpeed);
    out.printf(" threshold     : %d C\n",    hot.bedTempThreshold);
    out.println(" -- Cold chamber --");
    out.printf(" exhaust max   : %d %%\n",   cold.exhaustFanMax);
    out.printf(" exhaust min   : %d %%\n",   cold.exhaustFanMin);
    out.printf(" cold recirc   : %d %%\n",   cold.recircFanSpeed);
    out.printf(" max chamber   : %d C\n",    cold.maxChamberTemp);
    out.printf(" PID Kp/Ki/Kd  : %.2f / %.2f / %.2f\n",
              cold.pidKp, cold.pidKi, cold.pidKd);
    out.println(" -- Debug --");
    out.printf(" debug         : %s\n",  debug.debugMode       ? "on" : "off");
    out.printf(" chamber sim   : %.1f C\n", debug.debugChamberTemp);
    out.printf(" bed sim       : %.1f C\n", debug.debugBedTemp);
    out.printf(" manual        : %s\n",  debug.manualFanControl ? "on" : "off");
    out.printf(" manual heat   : %d %%\n",  debug.manualHeatingFanSpeed);
    out.printf(" manual exh    : %d %%\n",  debug.manualExhaustFanSpeed);
    out.printf(" manual rec    : %d %%\n",  debug.manualRecircFanSpeed);
    out.println(" -- Hardware --");
    out.printf(" heating fan   : %s  type: %s\n",
              debug.heatingFanPresent ? "yes" : "no",
              fanTypeToString(debug.heatingFanType));
    out.printf(" exhaust fan   : %s  type: %s\n",
              debug.exhaustFanPresent ? "yes" : "no",
              fanTypeToString(debug.exhaustFanType));
    out.printf(" recirc fan    : %s  type: %s\n",
              debug.recircFanPresent ? "yes" : "no",
              fanTypeToString(debug.recircFanType));
    out.printf(" chamber light : %s\n", debug.chamberLightPresent ? "yes" : "no");
    out.printf(" 2P/3P PWM Hz  : %d\n", lowPinPwmFreqHz);
    out.printf(" log interval  : %d min\n", debug.logIntervalMin);
    out.println(" -- Light --");
    out.printf(" light         : %s\n", chamberLightOn ? "on" : "off");
}
