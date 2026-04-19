#include "StateMachine.h"
#include "../logging/Logger.h"

// ---------------------------------------------------------------------------
// StateMachine.cpp
// ---------------------------------------------------------------------------

const char* controllerStateToString(ControllerState state) {
    switch (state) {
        case ControllerState::Idle:          return "IDLE";
        case ControllerState::Recirculating: return "RECIRCULATING";
        case ControllerState::Heating:       return "HEATING";
        case ControllerState::Cooling:       return "COOLING";
        case ControllerState::Manual:        return "MANUAL";
        default:                             return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------

StateMachine::StateMachine(FanController& exhaustFan,
                            FanController& recircFan,
                            FanController& heatingFan,
                            TemperatureSensors& sensors)
    : _exhaustFan(exhaustFan),
      _recircFan(recircFan),
      _heatingFan(heatingFan),
      _sensors(sensors),
      _state(ControllerState::Idle),
      _mdtStartMs(0),
      _mdtRunning(false),
      _chamberSensorError(false)
{}

void StateMachine::begin() {
    _enterIdle();
    Serial.println("[SM] State machine started in IDLE");
}

// ---------------------------------------------------------------------------
// Main update — called every FAN_UPDATE_INTERVAL_MS from TaskManager
// ---------------------------------------------------------------------------

void StateMachine::update() {
    OperatingMode mode = gSettings.operatingMode;

    // MANUAL mode — fan speeds set directly from per-fan settings
    if (mode == OperatingMode::Manual) {
        if (_state != ControllerState::Manual) {
            _state = ControllerState::Manual;
            _mdtRunning = false;
            _pid.reset();
            gSettings.debug.manualFanControl = true;
            Serial.println("[SM] -> MANUAL");
        }
        _heatingFan.setSpeed(gSettings.debug.manualHeatingFanSpeed);
        _exhaustFan.setSpeed(gSettings.debug.manualExhaustFanSpeed);
        _recircFan.setSpeed(gSettings.debug.manualRecircFanSpeed);
        return;
    }

    // Leaving MANUAL mode — clear manual flag
    if (_state == ControllerState::Manual) {
        gSettings.debug.manualFanControl = false;
        _enterIdle();
    }

    float bedTemp     = _getBedTemp();
    float chamberTemp = _getChamberTemp();

    // Track chamber sensor health.
    _chamberSensorError = !_sensors.isChamberOk() &&
                          !gSettings.debug.debugMode;

    // When the chamber sensor fails in COOLING the exhaust fan runs at max.
    if (_chamberSensorError && _state == ControllerState::Cooling
                             && gSettings.debug.exhaustFanPresent) {
        _exhaustFan.setSpeed(gSettings.cold.exhaustFanMax);
        Serial.println("[SM] Chamber sensor error — exhaust at max");
    }

    // -----------------------------------------------------------------------
    // Forced modes (HEAT / COOL): entered immediately, no RFSBT/MDT check on
    // entry. After MDT has elapsed, if bed temp is below RFSBT → go to IDLE.
    // -----------------------------------------------------------------------
    if (mode == OperatingMode::Heat) {
        if (_state != ControllerState::Heating) {
            _enterHeating();
        } else {
            _applyHeatingFanSpeeds();
            // Check if printing has stopped.
            if (_mdtRunning && _mdtExpired()) {
                _mdtRunning = false;
                if (bedTemp < gSettings.recircStartBedTemp) {
                    Serial.println("[SM] Forced HEAT: MDT expired, bed cold -> IDLE");
                    _enterIdle();
                }
            }
        }
        return;
    }

    if (mode == OperatingMode::Cool) {
        if (_state != ControllerState::Cooling) {
            _enterCooling();
        } else {
            _applyCoolingFanSpeeds();
            if (_mdtRunning && _mdtExpired()) {
                _mdtRunning = false;
                if (bedTemp < gSettings.recircStartBedTemp) {
                    Serial.println("[SM] Forced COOL: MDT expired, bed cold -> IDLE");
                    _enterIdle();
                }
            }
        }
        return;
    }

    // -----------------------------------------------------------------------
    // AUTO mode state machine
    // -----------------------------------------------------------------------
    switch (_state) {

        case ControllerState::Idle:
            if (bedTemp >= gSettings.recircStartBedTemp) {
                Serial.printf("[SM] Bed %.1f >= RFSBT %d -> RECIRCULATING\n",
                              bedTemp, gSettings.recircStartBedTemp);
                _enterRecirculating();
            }
            break;

        case ControllerState::Recirculating:
            if (_mdtExpired()) {
                _mdtRunning = false;
                // Re-read bed temp at decision moment.
                bedTemp = _getBedTemp();
                if (bedTemp < gSettings.recircStartBedTemp) {
                    // Printing stopped before decision completed.
                    Serial.println("[SM] MDT expired, bed cold -> IDLE");
                    _enterIdle();
                } else if (bedTemp >= gSettings.hot.bedTempThreshold) {
                    Serial.printf("[SM] MDT expired, bed %.1f >= hot threshold %d -> HEATING\n",
                                  bedTemp, gSettings.hot.bedTempThreshold);
                    if (_logger) _logger->notifyModeDecided(true);
                    _enterHeating();
                } else {
                    Serial.printf("[SM] MDT expired, bed %.1f < hot threshold %d -> COOLING\n",
                                  bedTemp, gSettings.hot.bedTempThreshold);
                    if (_logger) _logger->notifyModeDecided(false);
                    _enterCooling();
                }
            }
            break;

        case ControllerState::Heating:
            _applyHeatingFanSpeeds();
            if (bedTemp < gSettings.recircStartBedTemp) {
                Serial.println("[SM] Bed cooled below RFSBT -> IDLE");
                _enterIdle();
            }
            break;

        case ControllerState::Cooling:
            _applyCoolingFanSpeeds();
            if (bedTemp < gSettings.recircStartBedTemp) {
                Serial.println("[SM] Bed cooled below RFSBT -> IDLE");
                _enterIdle();
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// State entry helpers
// ---------------------------------------------------------------------------

void StateMachine::_enterIdle() {
    _state      = ControllerState::Idle;
    _mdtRunning = false;
    _exhaustFan.setSpeed(0);
    _recircFan.setSpeed(0);
    _heatingFan.setSpeed(0);
    _pid.reset();
    Serial.println("[SM] -> IDLE");
    if (_logger) _logger->notifyJobEnd();
}

void StateMachine::_enterRecirculating() {
    _state = ControllerState::Recirculating;
    _recircFan.setSpeed(gSettings.debug.recircFanPresent ? gSettings.recircStartSpeed : 0);
    _exhaustFan.setSpeed(0);
    _heatingFan.setSpeed(0);

    // Start the Mode Decision Timer.
    _mdtStartMs = millis();
    _mdtRunning = true;
    Serial.printf("[SM] -> RECIRCULATING  (MDT %d min)\n",
                  gSettings.modeDecisionTimeMin);
    if (_logger) _logger->notifyJobStart();
}

void StateMachine::_enterHeating() {
    _state = ControllerState::Heating;
    _pid.reset();

    // Start MDT when entering a forced mode so we can detect print-end.
    if (!_mdtRunning) {
        _mdtStartMs = millis();
        _mdtRunning = true;
    }

    _applyHeatingFanSpeeds();
    Serial.println("[SM] -> HEATING");
    if (_logger && !_logger->isJobActive()) {
        _logger->notifyJobStart();
        _logger->notifyModeDecided(true);   // forced HEATING = HOT
    }
}

void StateMachine::_enterCooling() {
    _state = ControllerState::Cooling;

    // Configure PID with current settings.
    _pid.configure(gSettings.cold.pidKp,
                   gSettings.cold.pidKi,
                   gSettings.cold.pidKd,
                   gSettings.cold.exhaustFanMin,
                   gSettings.cold.exhaustFanMax);
    _pid.setSetpoint(gSettings.cold.maxChamberTemp);
    _pid.reset();

    if (!_mdtRunning) {
        _mdtStartMs = millis();
        _mdtRunning = true;
    }

    _applyCoolingFanSpeeds();
    Serial.println("[SM] -> COOLING");
    if (_logger && !_logger->isJobActive()) {
        _logger->notifyJobStart();
        _logger->notifyModeDecided(false);  // forced COOLING = COLD
    }
}

// ---------------------------------------------------------------------------
// Per-tick fan speed application
// ---------------------------------------------------------------------------

void StateMachine::_applyHeatingFanSpeeds() {
    _exhaustFan.setSpeed(gSettings.debug.exhaustFanPresent  ? gSettings.hot.exhaustFanSpeed  : 0);
    _recircFan.setSpeed(gSettings.debug.recircFanPresent    ? gSettings.hot.recircFanSpeed   : 0);
    _heatingFan.setSpeed(gSettings.debug.heatingFanPresent  ? gSettings.hot.heatingFanSpeed  : 0);
}

void StateMachine::_applyCoolingFanSpeeds() {
    // Exhaust fan speed is PID-controlled (or max if sensor failed).
    if (gSettings.debug.exhaustFanPresent) {
        if (!_chamberSensorError) {
            double pidOut = _pid.compute(_getChamberTemp());
            _exhaustFan.setSpeed(static_cast<uint8_t>(pidOut));
        } else {
            _exhaustFan.setSpeed(gSettings.cold.exhaustFanMax);
        }
    } else {
        _exhaustFan.setSpeed(0);
    }
    _recircFan.setSpeed(gSettings.debug.recircFanPresent ? gSettings.cold.recircFanSpeed : 0);
    _heatingFan.setSpeed(0);  // Heating fan is OFF in COOLING
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float StateMachine::_getChamberTemp() const {
    if (gSettings.debug.debugMode) return gSettings.debug.debugChamberTemp;
    return _sensors.getChamberTemp();
}

float StateMachine::_getBedTemp() const {
    if (gSettings.debug.debugMode) return gSettings.debug.debugBedTemp;
    return _sensors.getBedTemp();
}

bool StateMachine::_mdtExpired() const {
    if (!_mdtRunning) return false;
    unsigned long elapsedMs = millis() - _mdtStartMs;
    return elapsedMs >= (static_cast<unsigned long>(gSettings.modeDecisionTimeMin) * 60000UL);
}
