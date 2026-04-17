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
// Debug: print encoder and button events to serial
// ============================================================
static void debugPrintInputs() {
    // TcMenu/IoAbstraction handles input internally.
    // We hook in via the switches callback for direct button debug output.
    // This is set up in debugSetupInputMonitor() below.
}

// Encoder change debug callback (registered after menuSetup)
static void onDebugEncoderChange(int newValue) {
    Serial.printf("[Debug] Encoder value: %d\n", newValue);
}

// Button event debug callback
static void onDebugButtonEvent(uint8_t pin, bool held) {
    Serial.printf("[Debug] Button pin %d %s\n", pin, held ? "HELD" : "PRESSED");
}

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
    gSettings.operatingMode =
        static_cast<OperatingMode>(menuGetConfiguredOperatingModeIndex());
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
        float ct = gSettings.debug.debugMode
                 ? gSettings.debug.debugChamberTemp
                 : sensors.getChamberTemp();
        menuUpdateFooterStatus(static_cast<uint8_t>(stateMachine.getState()), ct);
    }
    uint8_t modeIdx = static_cast<uint8_t>(stateMachine.getState());

    float bedTemp     = gSettings.debug.debugMode
                      ? gSettings.debug.debugBedTemp
                      : sensors.getBedTemp();
    float chamberTemp = gSettings.debug.debugMode
                      ? gSettings.debug.debugChamberTemp
                      : sensors.getChamberTemp();

    menuUpdateStatus(modeIdx,
                     bedTemp,
                     chamberTemp,
                     heatingFan.getSpeedPercent(),
                     recircFan.getSpeedPercent(),
                     exhaustFan.getSpeedPercent());
    menuSetWifiIpStatus(wifiManager.getStatusText());
}

static void taskUpdateFooter() {
    if (ScreenKeyboard::isActive()) return;   // keyboard owns the display
    float chamberTemp = gSettings.debug.debugMode
                      ? gSettings.debug.debugChamberTemp
                      : sensors.getChamberTemp();
    menuUpdateFooterStatus(static_cast<uint8_t>(stateMachine.getState()), chamberTemp);
}

// ============================================================
// Periodic task: log tick (fires every 60 s; Logger checks interval internally)
// ============================================================
static void taskLogTick() {
    float bedC = gSettings.debug.debugMode
               ? gSettings.debug.debugBedTemp
               : sensors.getBedTemp();
    float chamberC = gSettings.debug.debugMode
                   ? gSettings.debug.debugChamberTemp
                   : sensors.getChamberTemp();
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
                  gSettings.debug.debugMode ? gSettings.debug.debugBedTemp
                                            : sensors.getBedTemp(),
                  gSettings.debug.debugMode ? gSettings.debug.debugChamberTemp
                                            : sensors.getChamberTemp(),
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
// ============================================================
void wifiReconnect() {
    wifiManager.reconnect();
    menuSetWifiIpStatus(wifiManager.getStatusText());
    if (wifiManager.isConnected() && !webServerStarted) {
        webServer.begin();
        taskManager.scheduleFixedRate(50,
                                      [] { webServer.handleClient(); },
                                      TIME_MILLIS);
        webServerStarted = true;
    }
    Serial.printf("[WiFi] Reconnect: %s\n", wifiManager.getStatusText());
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
    // what was persisted previously.
    gSettings.debug.debugMode = false;
    gSettings.debug.manualFanControl = false;

    // Initialise sensors (first requestAll so the first update() has data)
    sensors.begin();
    sensors.requestAll();

    // Initialise fans
    exhaustFan.begin();
    recircFan.begin();
    heatingFan.begin();

    // Initialise Wi-Fi independently from the control logic.
    wifiManager.begin();

    // Start HTTP status server (only useful when WiFi is connected)
    if (wifiManager.isConnected()) {
        webServer.begin();
        webServerStarted = true;
    }

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

    // HTTP server — poll for incoming requests
    if (webServerStarted) {
        taskManager.scheduleFixedRate(50,
                                      [] { webServer.handleClient(); },
                                      TIME_MILLIS);
    }

    Serial.println("[Boot] Setup complete — entering main loop");
}

// ============================================================
// loop()
// Must only call taskManager.runLoop(). Everything else is in tasks.
// ============================================================
void loop() {
    taskManager.runLoop();
}
