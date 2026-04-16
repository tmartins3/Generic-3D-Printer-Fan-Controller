# Print Job Logging — Design Spec

**Date:** 2026-04-16
**Project:** FanController2 (ESP32 3D printer enclosure controller)

---

## Overview

Log temperature, fan RPM, and state machine data to LittleFS flash storage
during each print job. One log file is kept per job type (HOT / COLD).
Logs are retrievable via the serial debug interface.

---

## Filesystem

- **Library:** LittleFS (better wear levelling than SPIFFS, same API,
  included in ESP32 Arduino framework)
- **Partition:** existing `spiffs` partition (1408 KB)
- **Max file size:** 128 KB per file
- **Max files:** 3 (`active.log`, `HOT.log`, `COLD.log`)
- **Max total usage:** 384 KB (27% of partition)

---

## Files

| File | Purpose |
|------|---------|
| `/active.log` | In-progress log for the current print job |
| `/HOT.log` | Last completed hot-chamber print |
| `/COLD.log` | Last completed cold-chamber print |

Only one HOT.log and one COLD.log are kept at any time. Previous files are
silently overwritten when a new job of the same type completes.

---

## Job Lifecycle

### Job start triggers
- **AUTO mode:** `_enterRecirculating()` fires when bed temp exceeds RFSBT
- **Forced HEATING mode:** `_enterHeating()` fires (if no job already active)
- **Forced COOLING mode:** `_enterCooling()` fires (if no job already active)

### Job end trigger
- `_enterIdle()` fires — recirculation fan stops, bed has cooled below RFSBT

### Boot cleanup
On `Logger::begin()`, if `/active.log` exists it is deleted (interrupted
print from a previous power cycle).

### Rename on job end
When `notifyJobEnd()` is called:
- If mode was decided as HEATING (or job was forced HEATING): rename to
  `/HOT.log`
- If mode was decided as COOLING (or job was forced COOLING): rename to
  `/COLD.log`
- If mode was never decided (job ended during RECIRC before MDT expired):
  discard — delete `/active.log`

---

## Data Format

### Settings header (written once at job start)

```
# FanController2 Print Log
# Started: <minutes since boot>
# Op Mode: AUTO|HEATING|COOLING
# MDT: 10 min
# RFSBT: 45 C
# Recirc Start Speed: 30 %
# Hot: heating=50% recirc=50% exhaust=15% threshold=60C
# Cold: exhaustMax=100% exhaustMin=15% recirc=20% maxTemp=38C PID=2.00/0.50/1.00
# Fans: heating=YES exhaust=YES recirc=YES
# Light: YES
# Interval: 1 min
#
# min;mode;bedC;chamberC;recircRPM;exhaustRPM;heatingRPM
```

### Data rows

```
0;RECIRC;47.2;24.1;850;0;0
1;RECIRC;52.4;25.3;850;0;0
10;HEATING;68.1;35.2;1200;300;1800
11;HEATING;69.4;36.7;1200;310;1820
```

- Delimiter: `;`
- Minutes: integer, elapsed from job start (not boot)
- Temperatures: one decimal place (°C)
- Mode values: `RECIRC`, `HEATING`, `COOLING`
- RPM: integer; 0 if fan not present or speed is 0

### Truncation marker

When file reaches 128 KB, logging stops and this line is appended:

```
# LOG TRUNCATED AT 128KB
```

---

## New Setting: Logging Interval

| Property | Value |
|----------|-------|
| Name | Logging Interval |
| Location | Settings → Debug |
| Type | Analog (integer minutes) |
| Range | 1–60 min |
| Default | 5 min |
| Persisted | Yes (EEPROM with other settings) |
| EEPROM impact | Bumps `SETTINGS_MAGIC` to `0xFC04` |

---

## Architecture

### New files
- `src/logging/Logger.h` — class declaration
- `src/logging/Logger.cpp` — implementation

### Logger class interface

```cpp
class Logger {
public:
    void begin();   // mount LittleFS, delete active.log if exists
    void end();     // unmount LittleFS

    // Called from StateMachine
    void notifyJobStart();
    void notifyModeDecided(bool isHot);   // called when MDT expires
    void notifyJobEnd();

    // Called from TaskManager periodic task
    void tick(float bedC, float chamberC,
              uint16_t recircRpm, uint16_t exhaustRpm, uint16_t heatingRpm,
              ControllerState state);

    bool isJobActive() const;
};
```

### StateMachine changes
Four one-line additions — calls into Logger:

| Method | Logger call |
|--------|-------------|
| `_enterRecirculating()` | `logger.notifyJobStart()` |
| `_enterHeating()` | `logger.notifyJobStart()` (guarded: only if not active) |
| `_enterCooling()` | `logger.notifyJobStart()` (guarded: only if not active) |
| `_enterIdle()` | `logger.notifyJobEnd()` |
| MDT decision in `update()` | `logger.notifyModeDecided(isHot)` |

Logger is passed to `StateMachine::begin()` or held as a reference set in
`main.cpp` via a `setLogger()` method — to avoid circular includes.

### main.cpp changes
- Declare `static Logger logger` instance
- Call `logger.begin()` in `setup()`
- Schedule `logger.tick(...)` via `taskManager.scheduleFixedRate()` at
  `gSettings.debug.logIntervalMin * 60000` ms — rescheduled when the
  interval setting changes
- Pass logger reference to StateMachine

### Settings changes
- Add `uint8_t logIntervalMin` to `DebugSettings` (default 1)
- Bump `SETTINGS_MAGIC` to `0xFC04`

### MenuSetup changes
- Add `AnalogMenuItem menuDebugLogInterval` to Debug submenu
- Range 1–60, unit "min"
- Handle `ID_DEBUG_LOG_INTERVAL` in `onSettingChanged()`

---

## Serial Interface

New command group `log`:

| Command | Action |
|---------|--------|
| `log list` | List available files with size and row count |
| `log fetch hot` | Stream HOT.log to serial |
| `log fetch cold` | Stream COLD.log to serial |
| `log fetch active` | Stream active.log (if running) |
| `log delete hot` | Delete HOT.log |
| `log delete cold` | Delete COLD.log |

`log fetch` reads and prints one line at a time to avoid large RAM
allocations.

Example `log list` output:
```
-----------------------------------------------------
 Log files
-----------------------------------------------------
 active.log  :  2847 bytes  (running, 47 rows)
 HOT.log     : 14203 bytes  (312 rows)
 COLD.log    : not found
-----------------------------------------------------
```

---

## Error Handling

| Condition | Behaviour |
|-----------|-----------|
| LittleFS mount fails | Log warning to serial; logging silently disabled for session |
| File write error | Close file, disable logging for remainder of job; serial warning |
| File size reaches 128 KB | Append truncation marker; stop logging |
| Boot with active.log present | Delete it silently |
| Job ends in RECIRC (no mode decided) | Delete active.log, no rename |

---

## Capacity Summary

| Interval | 24h print rows | File size |
|----------|---------------|-----------|
| 1 min | 1 440 | ~58 KB |
| 5 min | 288 | ~12 KB |
| 60 min | 24 | ~1.5 KB |

128 KB limit covers a 1-minute interval print of ~52 hours — sufficient for
any realistic print job.
