#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// ControllerState.h
// Standalone enum for the state machine states + string conversion.
// Separated from StateMachine.h so lightweight consumers (Logger,
// StatusWebServer) don't pull in the full StateMachine dependency tree.
// ---------------------------------------------------------------------------

enum class ControllerState : uint8_t {
    Idle          = 0,
    Recirculating = 1,
    Heating       = 2,
    Cooling       = 3,
    Manual        = 4
};

const char* controllerStateToString(ControllerState state);
