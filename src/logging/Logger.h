#pragma once
#include <Arduino.h>
#include "../control/ControllerState.h"

// ---------------------------------------------------------------------------
// Logger.h
// Logs print job data to LittleFS. One file per job type:
//   /active.log  — in-progress job
//   /HOT.log     — last completed hot-chamber job
//   /COLD.log    — last completed cold-chamber job
//
// Call begin() once from setup().
// Call notifyJobStart/ModeDecided/JobEnd from StateMachine.
// Call tick() every 60 s from a TaskManager task.
// ---------------------------------------------------------------------------

#define LOG_MAX_BYTES  (128UL * 1024UL)   // 128 KB per file

class Logger {
public:
    // Mount LittleFS; delete /active.log if present (interrupted job cleanup).
    void begin();

    // --- Notifications from StateMachine ---

    // Call when a print job begins (entering RECIRC or forced HEAT/COOL).
    void notifyJobStart();

    // Call when AUTO mode decides on HOT or COLD after MDT expires.
    // Also call immediately after notifyJobStart() for forced modes:
    //   notifyModeDecided(true)  for forced HEATING
    //   notifyModeDecided(false) for forced COOLING
    void notifyModeDecided(bool isHot);

    // Call when the job ends (_enterIdle fires).
    void notifyJobEnd();

    // --- Periodic tick (called every 60 s from TaskManager) ---
    void tick(float bedC, float chamberC,
              uint16_t recircRpm, uint16_t exhaustRpm, uint16_t heatingRpm,
              ControllerState state);

    bool isJobActive() const { return _jobActive; }

private:
    // Returns true if the header was written successfully.
    bool     _writeHeader();
    void     _writeRow(float bedC, float chamberC,
                       uint16_t recircRpm, uint16_t exhaustRpm,
                       uint16_t heatingRpm, ControllerState state);
    uint32_t _elapsedMin() const;

    bool     _disabled     = false;  // true if LittleFS mount failed
    bool     _jobActive    = false;
    bool     _modeDecided  = false;
    bool     _isHot        = false;
    bool     _truncated    = false;
    bool     _forceWrite   = false;
    uint32_t _jobStartMs   = 0;
    uint32_t _lastWriteMs  = 0;
};
