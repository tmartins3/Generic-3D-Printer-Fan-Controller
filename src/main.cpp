#include <Arduino.h>
#include <TaskManagerIO.h>

#include "../include/Config.h"
#include "settings/Settings.h"
#include "sensors/TemperatureSensors.h"
#include "fans/FanController.h"
#include "control/StateMachine.h"
#include "menu/MenuSetup.h"
#include "network/WifiManager.h"
#include "debug/SerialInterface.h"
#include "logging/Logger.h"
#include "network/StatusWebServer.h"
#include "ui/ScreenKeyboard.h"

// ---------------------------------------------------------------------------
// main.cpp
// Wires all subsystems together. All periodic work is scheduled via
// TaskManagerIO so the main loop() contains only taskManager.runLoop().
// Never use delay() — it blocks the task manager and breaks TcMenu.
// ---------------------------------------------------------------------------

// ============================================================
// Subsystem instances
// ============================================================
static TemperatureSensors sensors;

// Fan instances: (pwmPin, tachPin, ledcChannel)
static FanController exhaustFan (PIN_EXHAUST_PWM,  PIN_EXHAUST_TACH,  LEDC_CH_EXHAUST);
static FanController recircFan  (PIN_RECIRC_PWM,   PIN_RECIRC_TACH,   LEDC_CH_RECIRC);
static FanController heatingFan (PIN_HEATING_PWM,  PIN_HEATING_TACH,  LEDC_CH_HEATING);

static StateMachine stateMachine(exhaustFan, recircFan, heatingFan, sensors);
static WifiManager wifiManager;
static SerialInterface serialInterface;
static Logger logger;
static StatusWebServer webServer;
static bool webServerStarted = false;

// ============================================================
// Periodic task: read temperature sensors
// Fires every TEMP_READ_INTERVAL_MS.
// requestAll() starts a non-blocking conversion; update() reads the result
// from the previous conversion. Since the interval (2000 ms) far exceeds
// the 12-bit conversion time (750 ms) the result is always ready.
// ============================================================
static void taskReadTemperatures() {
    sensors.update();     // read result of the previous conversion
    sensors.requestAll(); // start next conversion for the next tick
}

// ============================================================
// Periodic task: run state machine + update fan speeds
// ============================================================
static void taskUpdateControl() {
    // operatingMode is kept in sync by onSettingChanged(ID_OP_MODE) in the
    // menu callback and by serial set commands — no per-tick re-read needed.
    stateMachine.update();
}

// ============================================================
// Periodic task: update tachometer RPM counters
// ============================================================
static void taskUpdateTach() {
    exhaustFan.updateRpm();
    recircFan.updateRpm();
    heatingFan.updateRpm();
}

// ============================================================
// Periodic task: refresh read-only display items
// ============================================================
static bool _kbWasActive = false;   // tracks keyboard→normal transition

static void taskUpdateDisplayStatus() {
    if (ScreenKeyboard::isActive()) {
        _kbWasActive = true;
        return;   // keyboard owns the display
    }
    // Keyboard just closed — force immediate footer redraw
    if (_kbWasActive) {
        _kbWasActive = false;
        menuUpdateFooterStatus(static_cast<uint8_t>(stateMachine.getState()),
                               sensors.getEffectiveChamberTemp());
    }
    uint8_t modeIdx = static_cast<uint8_t>(stateMachine.getState());

    float bedTemp     = sensors.getEffectiveBedTemp();
    float chamberTemp = sensors.getEffectiveChamberTemp();

    menuUpdateStatus(modeIdx,
                     bedTemp,
                     chamberTemp,
                     heatingFan.getSpeedPercent(),
                     recircFan.getSpeedPercent(),
                     exhaustFan.getSpeedPercent());
    menuUpdateRpm(heatingFan.getRpm(), recircFan.getRpm(), exhaustFan.getRpm());
    menuSetWifiIpStatus(wifiManager.getStatusText());
}

static void taskUpdateFooter() {
    if (ScreenKeyboard::isActive()) return;   // keyboard owns the display
    float chamberTemp = sensors.getEffectiveChamberTemp();
    menuUpdateFooterStatus(static_cast<uint8_t>(stateMachine.getState()), chamberTemp);
}

// ============================================================
// Periodic task: log tick (fires every 60 s; Logger checks interval internally)
// ============================================================
static void taskLogTick() {
    float bedC     = sensors.getEffectiveBedTemp();
    float chamberC = sensors.getEffectiveChamberTemp();
    logger.tick(bedC, chamberC,
                recircFan.getRpm(), exhaustFan.getRpm(), heatingFan.getRpm(),
                stateMachine.getState());
}

// ============================================================
// Periodic task: serial status dump (debug convenience)
// ============================================================
static void taskSerialStatus() {
    Serial.printf("[Status] State=%-12s  Bed=%.1f°C  Chamber=%.1f°C  "
                  "Exhaust=%d%%  Recirc=%d%%  Heating=%d%%  "
                  "ExhaustRPM=%d  RecircRPM=%d  HeatingRPM=%d%s\n",
                  controllerStateToString(stateMachine.getState()),
                  sensors.getEffectiveBedTemp(),
                  sensors.getEffectiveChamberTemp(),
                  exhaustFan.getSpeedPercent(),
                  recircFan.getSpeedPercent(),
                  heatingFan.getSpeedPercent(),
                  exhaustFan.getRpm(),
                  recircFan.getRpm(),
                  heatingFan.getRpm(),
                  stateMachine.hasChamberSensorError() ? "  [CHAMBER SENSOR ERROR]" : "");
}

// ============================================================
// wifiReconnect — called from menu after keyboard edits credentials
// Non-blocking: starts connection attempt, poll task handles the rest.
// ============================================================
void wifiReconnect() {
    wifiManager.reconnect();
    menuSetWifiIpStatus(wifiManager.getStatusText());
    Serial.printf("[WiFi] Reconnect started: %s\n", wifiManager.getStatusText());
}

// ============================================================
// applyFanPresence — sync fan present and invert flags from settings
// ============================================================
void applyFanPresence() {
    exhaustFan.setPresent(gSettings.debug.exhaustFanPresent);
    recircFan.setPresent(gSettings.debug.recircFanPresent);
    heatingFan.setPresent(gSettings.debug.heatingFanPresent);
    exhaustFan.setInvertPwm(gSettings.debug.exhaustFanInvertPwm);
    recircFan.setInvertPwm(gSettings.debug.recircFanInvertPwm);
    heatingFan.setInvertPwm(gSettings.debug.heatingFanInvertPwm);
}

// ============================================================
// applyFanFrequencies — set PWM freq per fan type
// ============================================================
void applyFanFrequencies() {
    uint32_t lowFreq  = gSettings.lowPinPwmFreqHz;
    uint32_t highFreq = FAN_PWM_FREQ_HZ;  // 25 kHz for 4PIN

    heatingFan.setFrequency(gSettings.debug.heatingFanType == FanType::Pin4 ? highFreq : lowFreq);
    exhaustFan.setFrequency(gSettings.debug.exhaustFanType == FanType::Pin4 ? highFreq : lowFreq);
    recircFan.setFrequency(gSettings.debug.recircFanType   == FanType::Pin4 ? highFreq : lowFreq);
}

// ============================================================
// setup()
// ============================================================
void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println("\n[Boot] FanController2 starting...");

    // Load settings (or write defaults if EEPROM is blank)
    if (!gSettings.load()) {
        Serial.println("[Boot] No saved settings — writing defaults");
        gSettings.save();
    } else {
        Serial.println("[Boot] Settings loaded from EEPROM");
    }

    // Always start with debug and manual fan override disabled, regardless of
    // what was persisted previously. Reset MANUAL mode to AUTO on boot.
    gSettings.debug.debugMode = false;
    gSettings.debug.manualFanControl = false;
    if (gSettings.operatingMode == OperatingMode::Manual) {
        gSettings.operatingMode = OperatingMode::Auto;
    }

    // Initialise sensors (first requestAll so the first update() has data)
    sensors.begin();
    sensors.requestAll();

    // Initialise fans and sync presence flags from settings
    exhaustFan.begin();
    recircFan.begin();
    heatingFan.begin();
    applyFanPresence();

    // Apply PWM frequency based on fan type (2PIN/3PIN use lower freq)
    applyFanFrequencies();

    // Initialise Wi-Fi (non-blocking — poll task checks connection progress).
    wifiManager.begin();

    // Initialise TcMenu display + encoder
    menuSetup();
    menuSetWifiIpStatus(wifiManager.getStatusText());

    // Initialise state machine (enters IDLE, all fans off)
    stateMachine.begin();
    logger.begin();
    stateMachine.setLogger(&logger);

    // Initialise serial debug interface
    serialInterface.begin(stateMachine, exhaustFan, recircFan, heatingFan, sensors);

    // Draw the initial status/footer immediately instead of waiting for the
    // first scheduled display refresh.
    taskUpdateDisplayStatus();
    taskUpdateFooter();

    // --------------------------------------------------------
    // Schedule periodic tasks via TaskManagerIO.
    // All intervals are in milliseconds.
    // --------------------------------------------------------

    // Temperature: request + read cycle
    taskManager.scheduleFixedRate(TEMP_READ_INTERVAL_MS,
                                  taskReadTemperatures,
                                  TIME_MILLIS);

    // State machine + fan control
    taskManager.scheduleFixedRate(FAN_UPDATE_INTERVAL_MS,
                                  taskUpdateControl,
                                  TIME_MILLIS);

    // Tachometer RPM update
    taskManager.scheduleFixedRate(TACH_MEASURE_MS,
                                  taskUpdateTach,
                                  TIME_MILLIS);

    // Display status refresh
    taskManager.scheduleFixedRate(500,
                                  taskUpdateDisplayStatus,
                                  TIME_MILLIS);

    // Footer status refresh
    taskManager.scheduleFixedRate(5000,
                                  taskUpdateFooter,
                                  TIME_MILLIS);

    // Serial status dump every 5 seconds
    taskManager.scheduleFixedRate(5000,
                                  taskSerialStatus,
                                  TIME_MILLIS);

    // Serial command interface — poll for incoming bytes
    taskManager.scheduleFixedRate(50,
                                  [] { serialInterface.poll(); },
                                  TIME_MILLIS);

    // Logger tick — fires every 60 s; Logger checks interval internally
    taskManager.scheduleFixedRate(60000, taskLogTick, TIME_MILLIS);

    // WiFi poll — checks connection progress and starts web server on connect
    taskManager.scheduleFixedRate(500, [] {
        if (wifiManager.poll()) {
            // Just connected — start web server if not already running
            if (!webServerStarted) {
                webServer.begin();
                taskManager.scheduleFixedRate(50,
                                              [] { webServer.handleClient(); },
                                              TIME_MILLIS);
                webServerStarted = true;
            }
        }
    }, TIME_MILLIS);

    Serial.println("[Boot] Setup complete — entering main loop");
}

// ============================================================
// loop()
// Must only call taskManager.runLoop(). Everything else is in tasks.
// ============================================================
void loop() {
    taskManager.runLoop();
}
