#pragma once

#include <Arduino.h>
#include "ControllerState.h"
#include "../fans/FanController.h"
#include "../sensors/TemperatureSensors.h"
#include "../settings/Settings.h"
#include "PidController.h"

class Logger;  // forward declaration — avoids circular include

// ---------------------------------------------------------------------------
// StateMachine.h
// Implements the five-state fan controller logic:
//
//   IDLE -> RECIRCULATING -> HEATING or COOLING -> IDLE
//   MANUAL (bypasses automatic sequence)
//
// State transitions are driven by bed temperature and the Mode Decision
// Timer (MDT). The operating mode setting (AUTO / HEAT / COOL / MANUAL)
// modifies behaviour as described in the project spec.
// ---------------------------------------------------------------------------

class StateMachine {
public:
    // References to the three fan controllers and the sensor module.
    StateMachine(FanController& exhaustFan,
                 FanController& recircFan,
                 FanController& heatingFan,
                 TemperatureSensors& sensors);

    // Call once during setup().
    void begin();

    // Call from a periodic TaskManager task (e.g. every 500 ms).
    // Reads temperatures, evaluates transitions, and updates fan speeds.
    void update();

    // Return the current controller state.
    ControllerState getState() const { return _state; }

    // True if the chamber sensor has failed (used to drive UI error indicator).
    bool hasChamberSensorError() const { return _chamberSensorError; }

    void setLogger(Logger* logger) { _logger = logger; }

private:
    FanController&     _exhaustFan;
    FanController&     _recircFan;
    FanController&     _heatingFan;
    TemperatureSensors& _sensors;
    PidController      _pid;

    ControllerState    _state;
    unsigned long      _mdtStartMs;   // Millis when MDT timer started
    bool               _mdtRunning;
    bool               _chamberSensorError;
    Logger*            _logger = nullptr;

    // --- Internal helpers ---------------------------------------------------

    // Read temperatures respecting debug mode.
    float _getChamberTemp() const;
    float _getBedTemp()     const;

    // Transition to a new state and apply the corresponding fan speeds.
    void _enterIdle();
    void _enterRecirculating();
    void _enterHeating();
    void _enterCooling();

    // Apply fan speeds for the current active state (called every update tick).
    void _applyHeatingFanSpeeds();
    void _applyCoolingFanSpeeds();

    // Check whether the MDT period has elapsed.
    bool _mdtExpired() const;
};
