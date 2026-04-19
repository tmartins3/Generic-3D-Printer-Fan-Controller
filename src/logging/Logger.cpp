#include "Logger.h"
#include "../../include/Config.h"
#include "../settings/Settings.h"
#include <LittleFS.h>

static const char* PATH_ACTIVE = "/active.log";
static const char* PATH_HOT    = "/HOT.log";
static const char* PATH_COLD   = "/COLD.log";

static const char* _stateStr(ControllerState s) {
    switch (s) {
        case ControllerState::Recirculating: return "RECIRC";
        case ControllerState::Heating:       return "HEATING";
        case ControllerState::Cooling:       return "COOLING";
        default:                             return "IDLE";
    }
}

void Logger::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[Log] LittleFS mount failed — logging disabled");
        _disabled = true;
        return;
    }
    if (LittleFS.exists(PATH_ACTIVE)) {
        LittleFS.remove(PATH_ACTIVE);
        Serial.println("[Log] Removed interrupted active.log");
    }
    Serial.println("[Log] LittleFS ready");
}

void Logger::notifyJobStart() {
    if (_disabled || _jobActive) return;
    _jobActive   = false;
    _modeDecided = false;
    _isHot       = false;
    _truncated   = false;
    _forceWrite  = true;
    _jobStartMs  = millis();
    _lastWriteMs = 0;
    if (!_writeHeader()) {
        Serial.println("[Log] Failed to write header — job not started");
        return;
    }
    _jobActive   = true;
    Serial.println("[Log] Job started");
}

void Logger::notifyModeDecided(bool isHot) {
    if (!_jobActive) return;
    _modeDecided = true;
    _isHot       = isHot;
    Serial.printf("[Log] Mode decided: %s\n", isHot ? "HOT" : "COLD");
}

void Logger::notifyJobEnd() {
    if (!_jobActive) return;
    _jobActive = false;

    if (!_modeDecided) {
        LittleFS.remove(PATH_ACTIVE);
        Serial.println("[Log] Job ended before mode decided — discarded");
        return;
    }

    const char* dest = _isHot ? PATH_HOT : PATH_COLD;
    if (LittleFS.exists(dest)) LittleFS.remove(dest);
    LittleFS.rename(PATH_ACTIVE, dest);
    Serial.printf("[Log] Job saved as %s\n", dest);
}

void Logger::tick(float bedC, float chamberC,
                  uint16_t recircRpm, uint16_t exhaustRpm, uint16_t heatingRpm,
                  ControllerState state) {
    if (!_jobActive || _truncated) return;

    uint32_t now      = millis();
    uint32_t interval = (uint32_t)gSettings.debug.logIntervalMin * 60000UL;
    bool     due      = _forceWrite || (now - _lastWriteMs) >= interval;
    if (!due) return;

    _forceWrite  = false;
    _lastWriteMs = now;
    _writeRow(bedC, chamberC, recircRpm, exhaustRpm, heatingRpm, state);
}

bool Logger::_writeHeader() {
    File f = LittleFS.open(PATH_ACTIVE, "w");
    if (!f) {
        Serial.println("[Log] Failed to open active.log for write");
        return false;
    }
    f.println("# FanController2 Print Log");
    f.printf("# Started: %lu min since boot\n", millis() / 60000UL);

    f.printf("# Op Mode: %s\n", operatingModeToString(gSettings.operatingMode));
    f.printf("# MDT: %d min\n",               gSettings.modeDecisionTimeMin);
    f.printf("# RFSBT: %d C\n",               gSettings.recircStartBedTemp);
    f.printf("# Recirc Start Speed: %d %%\n", gSettings.recircStartSpeed);
    f.printf("# Hot: heating=%d%% recirc=%d%% exhaust=%d%% threshold=%dC\n",
             gSettings.hot.heatingFanSpeed, gSettings.hot.recircFanSpeed,
             gSettings.hot.exhaustFanSpeed, gSettings.hot.bedTempThreshold);
    f.printf("# Cold: exhaustMax=%d%% exhaustMin=%d%% recirc=%d%% "
             "maxTemp=%dC PID=%.2f/%.2f/%.2f\n",
             gSettings.cold.exhaustFanMax,  gSettings.cold.exhaustFanMin,
             gSettings.cold.recircFanSpeed, gSettings.cold.maxChamberTemp,
             gSettings.cold.pidKp, gSettings.cold.pidKi, gSettings.cold.pidKd);
    f.printf("# Fans: heating=%s exhaust=%s recirc=%s\n",
             gSettings.debug.heatingFanPresent ? "YES" : "NO",
             gSettings.debug.exhaustFanPresent ? "YES" : "NO",
             gSettings.debug.recircFanPresent  ? "YES" : "NO");
    f.printf("# Light: %s\n",        gSettings.debug.chamberLightPresent ? "YES" : "NO");
    f.printf("# Interval: %d min\n", gSettings.debug.logIntervalMin);
    f.println("#");
    f.println("# min;mode;bedC;chamberC;recircRPM;exhaustRPM;heatingRPM");
    f.close();
    return true;
}

void Logger::_writeRow(float bedC, float chamberC,
                        uint16_t recircRpm, uint16_t exhaustRpm,
                        uint16_t heatingRpm, ControllerState state) {
    // Check size before opening for append to avoid writing past the limit
    {
        File check = LittleFS.open(PATH_ACTIVE, "r");
        size_t sz = check ? check.size() : 0;
        if (check) check.close();
        if (sz >= LOG_MAX_BYTES) {
            File f = LittleFS.open(PATH_ACTIVE, "a");
            if (f) {
                f.printf("# LOG TRUNCATED AT %luKB\n", LOG_MAX_BYTES / 1024UL);
                f.close();
            }
            _truncated = true;
            Serial.println("[Log] File size limit reached — logging stopped");
            return;
        }
    }

    File f = LittleFS.open(PATH_ACTIVE, "a");
    if (!f) return;

    f.printf("%lu;%s;%.1f;%.1f;%u;%u;%u\n",
             _elapsedMin(), _stateStr(state),
             bedC, chamberC,
             recircRpm, exhaustRpm, heatingRpm);
    f.close();
}

uint32_t Logger::_elapsedMin() const {
    return (millis() - _jobStartMs) / 60000UL;
}
