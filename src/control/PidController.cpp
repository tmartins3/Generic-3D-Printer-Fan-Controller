#include "PidController.h"
#include "../../include/Config.h"

// ---------------------------------------------------------------------------
// PidController.cpp
// ---------------------------------------------------------------------------

PidController::PidController()
    : _input(25.0),
      _output(0.0),
      _setpoint(38.0),
      // REVERSE: output increases when input rises above setpoint.
      _pid(&_input, &_output, &_setpoint, 2.0, 0.5, 1.0, REVERSE)
{
    _pid.SetMode(AUTOMATIC);
    _pid.SetOutputLimits(0.0, 100.0);  // Default; overridden by configure()
    _pid.SetSampleTime(PID_INTERVAL_MS);
}

void PidController::configure(double kp, double ki, double kd,
                               double outMin, double outMax) {
    _pid.SetTunings(kp, ki, kd);
    _pid.SetOutputLimits(outMin, outMax);
}

void PidController::setSetpoint(double setpointCelsius) {
    _setpoint = setpointCelsius;
}

double PidController::compute(double chamberTempCelsius) {
    _input = chamberTempCelsius;
    _pid.Compute();  // Only updates _output when the sample time has elapsed
    return _output;
}

void PidController::reset() {
    // Switch to MANUAL briefly to reset the integrator, then back to AUTOMATIC.
    _pid.SetMode(MANUAL);
    _output = 0.0;
    _pid.SetMode(AUTOMATIC);
}
