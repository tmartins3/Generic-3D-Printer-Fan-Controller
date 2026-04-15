#pragma once

#include <Arduino.h>
#include "../fans/FanController.h"
#include "../sensors/TemperatureSensors.h"
#include "../settings/Settings.h"
#include "PidController.h"

// ---------------------------------------------------------------------------
// StateMachine.h
// Implements the four-state fan controller logic:
//
//   IDLE -> RECIRCULATING -> HEATING or COOLING -> IDLE
//
// State transitions are driven by bed temperature and the Mode Decision
// Timer (MDT). The operating mode setting (AUTO / HEAT / COOL) modifies
// behaviour as described in the project spec.
// ---------------------------------------------------------------------------

enum class ControllerState : uint8_t {
    Idle          = 0,
    Recirculating = 1,
    Heating       = 2,
    Cooling       = 3
};

// Human-readable names for serial debug output.
const char* controllerStateToString(ControllerState state);

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
