#pragma once

// ---------------------------------------------------------------------------
// SerialInterface.h
// Simple serial command interface for testing and debugging.
//
// Call begin() once from setup(), then schedule poll() as a periodic task.
//
// Commands (type into serial monitor, end with Enter):
//   help                          — list all commands
//   status                        — live state, temps, fan speeds and RPM
//   get                           — dump all current settings
//   set debug     on|off          — enable/disable debug mode (simulated temps)
//   set chamber   <°C>            — debug chamber temperature
//   set bed       <°C>            — debug bed temperature
//   set manual    on|off          — enable/disable manual fan control
//   set heating   <0-100>         — manual heating fan speed %
//   set exhaust   <0-100>         — manual exhaust fan speed %
//   set recirc    <0-100>         — manual recirc fan speed %
//   set mode      auto|heating|cooling  — operating mode
//   set mdt       <minutes>       — mode decision time
//   set rfsbt     <°C>            — recirc start bed temp (startup threshold)
//   set threshold <°C>            — hot chamber bed temp threshold
//   set light     on|off          — chamber light
//   log list              — list log files with size and row count
//   log fetch hot|cold|active — stream log file to serial
//   log delete hot|cold   — delete log file
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "../control/StateMachine.h"
#include "../fans/FanController.h"
#include "../sensors/TemperatureSensors.h"

class SerialInterface {
public:
    void begin(StateMachine&       sm,
               FanController&      exhaust,
               FanController&      recirc,
               FanController&      heating,
               TemperatureSensors& sensors);

    // Call from a periodic TaskManager task (every ~50 ms)
    void poll();

private:
    void _processLine(char* line);
    void _cmdHelp();
    void _cmdStatus();
    void _cmdGet();
    void _cmdSet(const char* key, const char* value);
    void _cmdLog(const char* sub, const char* arg);

    static void _ok(const char* msg = nullptr);
    static void _err(const char* msg);

    StateMachine*       _sm      = nullptr;
    FanController*      _exhaust = nullptr;
    FanController*      _recirc  = nullptr;
    FanController*      _heating = nullptr;
    TemperatureSensors* _sensors = nullptr;

    static constexpr uint8_t BUF_SIZE = 80;
    char    _buf[BUF_SIZE];
    uint8_t _len = 0;
};
