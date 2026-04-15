#pragma once

#include <Arduino.h>
#include <PID_v1.h>

// ---------------------------------------------------------------------------
// PidController.h
// Thin wrapper around the Arduino-PID-Library (br3ttb/Arduino-PID-Library).
//
// Used to regulate the exhaust fan speed during COOLING mode.
//   - Input:    chamber temperature (°C)
//   - Setpoint: Max cold chamber temperature setting
//   - Output:   exhaust fan speed (%), clamped to [exhaustFanMin, exhaustFanMax]
//
// The PID acts in REVERSE direction: when temperature rises above setpoint
// the output (fan speed) must increase.
// ---------------------------------------------------------------------------

class PidController {
public:
    PidController();

    // Configure gains and output limits. Call whenever settings change.
    void configure(double kp, double ki, double kd,
                   double outMin, double outMax);

    // Update the setpoint (Max cold chamber temperature).
    void setSetpoint(double setpointCelsius);

    // Run one PID iteration with the current chamber temperature.
    // Returns the computed fan speed percent (clamped to outMin/outMax).
    double compute(double chamberTempCelsius);

    // Reset integrator and internal state (call when entering/leaving COOLING).
    void reset();

private:
    double _input;
    double _output;
    double _setpoint;
    PID    _pid;
};
