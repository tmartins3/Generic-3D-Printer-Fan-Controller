#include "StatusWebServer.h"
#include "../settings/Settings.h"
#include "../control/StateMachine.h"
#include <WebServer.h>
#include <LittleFS.h>

// ---------------------------------------------------------------------------
// StatusWebServer.cpp
// ---------------------------------------------------------------------------

static WebServer server(80);
static StatusWebServer* _instance = nullptr;

void StatusWebServer::begin() {
    _instance = this;
    server.on("/", HTTP_GET, []() { _instance->_handleRoot(); });
    server.begin();
    Serial.println("[Web] HTTP server started on port 80");
}

void StatusWebServer::handleClient() {
    server.handleClient();
}

// ---------------------------------------------------------------------------
// GET /  — single plain-text page with settings + log files
// ---------------------------------------------------------------------------
void StatusWebServer::_handleRoot() {
    String out;
    out.reserve(4096);

    out += "=====================================================\n";
    out += " FanController2 Status\n";
    out += "=====================================================\n\n";

    _appendSettings(out);

    out += "\n\n=====================================================\n";
    out += " HOT.log\n";
    out += "=====================================================\n\n";
    _appendLogFile(out, "/HOT.log", "HOT");

    out += "\n\n=====================================================\n";
    out += " COLD.log\n";
    out += "=====================================================\n\n";
    _appendLogFile(out, "/COLD.log", "COLD");

    server.send(200, "text/plain", out);
}

// ---------------------------------------------------------------------------
// Settings dump (mirrors serial "get" command output)
// ---------------------------------------------------------------------------
void StatusWebServer::_appendSettings(String& out) {
    const auto& s = gSettings;

    out += "-- General --\n";
    out += " mode          : ";
    out += (s.operatingMode == OperatingMode::Auto ? "auto" :
            s.operatingMode == OperatingMode::Heat ? "heating" : "cooling");
    out += "\n";

    char buf[80];

    snprintf(buf, sizeof(buf), " mdt           : %d min\n", s.modeDecisionTimeMin);
    out += buf;
    snprintf(buf, sizeof(buf), " rfsbt         : %d C\n", s.recircStartBedTemp);
    out += buf;
    snprintf(buf, sizeof(buf), " recirc speed  : %d %%\n", s.recircStartSpeed);
    out += buf;

    out += "-- Hot chamber --\n";
    snprintf(buf, sizeof(buf), " heating fan   : %d %%\n", s.hot.heatingFanSpeed);
    out += buf;
    snprintf(buf, sizeof(buf), " hot recirc    : %d %%\n", s.hot.recircFanSpeed);
    out += buf;
    snprintf(buf, sizeof(buf), " hot exhaust   : %d %%\n", s.hot.exhaustFanSpeed);
    out += buf;
    snprintf(buf, sizeof(buf), " threshold     : %d C\n", s.hot.bedTempThreshold);
    out += buf;

    out += "-- Cold chamber --\n";
    snprintf(buf, sizeof(buf), " exhaust max   : %d %%\n", s.cold.exhaustFanMax);
    out += buf;
    snprintf(buf, sizeof(buf), " exhaust min   : %d %%\n", s.cold.exhaustFanMin);
    out += buf;
    snprintf(buf, sizeof(buf), " cold recirc   : %d %%\n", s.cold.recircFanSpeed);
    out += buf;
    snprintf(buf, sizeof(buf), " max chamber   : %d C\n", s.cold.maxChamberTemp);
    out += buf;
    snprintf(buf, sizeof(buf), " PID Kp/Ki/Kd  : %.2f / %.2f / %.2f\n",
             s.cold.pidKp, s.cold.pidKi, s.cold.pidKd);
    out += buf;

    out += "-- Debug --\n";
    snprintf(buf, sizeof(buf), " debug         : %s\n", s.debug.debugMode ? "on" : "off");
    out += buf;
    snprintf(buf, sizeof(buf), " manual        : %s\n", s.debug.manualFanControl ? "on" : "off");
    out += buf;
    snprintf(buf, sizeof(buf), " log interval  : %d min\n", s.debug.logIntervalMin);
    out += buf;

    out += "-- Hardware presence --\n";
    snprintf(buf, sizeof(buf), " heating fan   : %s\n", s.debug.heatingFanPresent ? "yes" : "no");
    out += buf;
    snprintf(buf, sizeof(buf), " exhaust fan   : %s\n", s.debug.exhaustFanPresent ? "yes" : "no");
    out += buf;
    snprintf(buf, sizeof(buf), " recirc fan    : %s\n", s.debug.recircFanPresent ? "yes" : "no");
    out += buf;
    snprintf(buf, sizeof(buf), " chamber light : %s\n", s.debug.chamberLightPresent ? "yes" : "no");
    out += buf;

    out += "-- Light --\n";
    snprintf(buf, sizeof(buf), " light         : %s\n", s.chamberLightOn ? "on" : "off");
    out += buf;
}

// ---------------------------------------------------------------------------
// Append a log file's contents, or a placeholder if missing
// ---------------------------------------------------------------------------
void StatusWebServer::_appendLogFile(String& out, const char* path, const char* label) {
    if (!LittleFS.exists(path)) {
        out += "LOGFILE NOT PRESENT\n";
        return;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        out += "LOGFILE NOT PRESENT\n";
        return;
    }
    while (f.available()) {
        out += f.readStringUntil('\n');
        out += '\n';
    }
    f.close();
}
