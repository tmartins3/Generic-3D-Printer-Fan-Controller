#include "SerialInterface.h"
#include "../settings/Settings.h"
#include "../menu/MenuSetup.h"
#include "../../include/Config.h"
#include <string.h>
#include <stdlib.h>
#include <LittleFS.h>

// ---------------------------------------------------------------------------
// SerialInterface.cpp
// ---------------------------------------------------------------------------

void SerialInterface::begin(StateMachine&       sm,
                            FanController&      exhaust,
                            FanController&      recirc,
                            FanController&      heating,
                            TemperatureSensors& sensors) {
    _sm      = &sm;
    _exhaust = &exhaust;
    _recirc  = &recirc;
    _heating = &heating;
    _sensors = &sensors;

    Serial.println("[Serial] Debug interface ready. Type 'help' for commands.");
}

// ---------------------------------------------------------------------------
// poll() — accumulate bytes into _buf; process on newline or carriage return
// ---------------------------------------------------------------------------
void SerialInterface::poll() {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());

        if (c == '\r') continue;   // ignore CR in CRLF pairs

        if (c == '\n') {
            _buf[_len] = '\0';
            if (_len > 0) {
                _processLine(_buf);
            }
            _len = 0;
            return;
        }

        if (_len < BUF_SIZE - 1) {
            _buf[_len++] = c;
        }
    }
}

// ---------------------------------------------------------------------------
// _processLine — tokenise and dispatch
// ---------------------------------------------------------------------------
void SerialInterface::_processLine(char* line) {
    // Strip leading whitespace
    while (*line == ' ') line++;
    if (*line == '\0') return;

    // First token: command
    char* cmd = strtok(line, " ");
    if (!cmd) return;

    if (strcasecmp(cmd, "help") == 0) {
        _cmdHelp();
    } else if (strcasecmp(cmd, "status") == 0) {
        _cmdStatus();
    } else if (strcasecmp(cmd, "get") == 0) {
        _cmdGet();
    } else if (strcasecmp(cmd, "set") == 0) {
        char* key = strtok(nullptr, " ");
        char* val = strtok(nullptr, " ");
        if (!key || !val) {
            _err("usage: set <key> <value>");
        } else {
            _cmdSet(key, val);
        }
    } else if (strcasecmp(cmd, "log") == 0) {
        char* sub = strtok(nullptr, " ");
        char* arg = strtok(nullptr, " ");
        _cmdLog(sub, arg);
    } else {
        Serial.printf("[Serial] Unknown command '%s'. Type 'help'.\n", cmd);
    }
}

// ---------------------------------------------------------------------------
// help
// ---------------------------------------------------------------------------
void SerialInterface::_cmdHelp() {
    Serial.println("-----------------------------------------------------");
    Serial.println(" Serial debug interface — available commands");
    Serial.println("-----------------------------------------------------");
    Serial.println(" help                         this message");
    Serial.println(" status                       live state, temps, fans");
    Serial.println(" get                          dump all settings");
    Serial.println(" set debug     on|off         simulated sensor mode");
    Serial.println(" set chamber   <float °C>     debug chamber temp");
    Serial.println(" set bed       <float °C>     debug bed temp");
    Serial.println(" set manual    on|off         manual fan override");
    Serial.println(" set heating   <0-100>        manual heating fan %");
    Serial.println(" set exhaust   <0-100>        manual exhaust fan %");
    Serial.println(" set recirc    <0-100>        manual recirc fan %");
    Serial.println(" set mode      auto|heating|cooling");
    Serial.println(" set mdt       <minutes>      mode decision time");
    Serial.println(" set rfsbt     <°C>           recirc start bed temp");
    Serial.println(" set threshold <°C>           hot chamber bed threshold");
    Serial.println(" set hfp       on|off         heating fan present");
    Serial.println(" set efp       on|off         exhaust fan present");
    Serial.println(" set rfp       on|off         recirc fan present");
    Serial.println(" set light     on|off         chamber light");
    Serial.println(" log list                     list log files");
    Serial.println(" log fetch hot|cold|active    stream log file to serial");
    Serial.println(" log delete hot|cold          delete log file");
    Serial.println("-----------------------------------------------------");
}

// ---------------------------------------------------------------------------
// status
// ---------------------------------------------------------------------------
void SerialInterface::_cmdStatus() {
    const bool dbg = gSettings.debug.debugMode;

    float bed     = _sensors->getEffectiveBedTemp();
    float chamber = _sensors->getEffectiveChamberTemp();

    Serial.println("-----------------------------------------------------");
    Serial.printf(" State   : %s\n", controllerStateToString(_sm->getState()));
    Serial.printf(" Bed     : %.1f C%s\n", bed,     dbg ? " [SIM]" : "");
    Serial.printf(" Chamber : %.1f C%s\n", chamber, dbg ? " [SIM]" : "");
    if (_sm->hasChamberSensorError())
        Serial.println(" *** CHAMBER SENSOR ERROR ***");
    Serial.println("-----------------------------------------------------");
    Serial.printf(" Exhaust : %3d %%   %5d RPM\n",
                  _exhaust->getSpeedPercent(), _exhaust->getRpm());
    Serial.printf(" Recirc  : %3d %%   %5d RPM\n",
                  _recirc->getSpeedPercent(),  _recirc->getRpm());
    Serial.printf(" Heating : %3d %%   %5d RPM\n",
                  _heating->getSpeedPercent(), _heating->getRpm());
    Serial.println("-----------------------------------------------------");
    Serial.printf(" Debug mode    : %s\n", gSettings.debug.debugMode       ? "ON" : "OFF");
    Serial.printf(" Manual fans   : %s\n", gSettings.debug.manualFanControl ? "ON" : "OFF");
    Serial.printf(" Chamber light : %s\n", gSettings.chamberLightOn         ? "ON" : "OFF");
    Serial.printf(" Op mode       : %s\n",
                  operatingModeToString(gSettings.operatingMode));
    Serial.println("-----------------------------------------------------");
}

// ---------------------------------------------------------------------------
// get — dump all settings
// ---------------------------------------------------------------------------
void SerialInterface::_cmdGet() {
    Serial.println("-----------------------------------------------------");
    Serial.println(" Settings");
    Serial.println("-----------------------------------------------------");
    gSettings.dump(Serial);
    Serial.println("-----------------------------------------------------");
}

// ---------------------------------------------------------------------------
// set
// ---------------------------------------------------------------------------
void SerialInterface::_cmdSet(const char* key, const char* value) {
    auto isOn  = [](const char* v) { return strcasecmp(v, "on")  == 0 || strcasecmp(v, "1") == 0; };
    auto isOff = [](const char* v) { return strcasecmp(v, "off") == 0 || strcasecmp(v, "0") == 0; };

    // --- debug mode ---
    if (strcasecmp(key, "debug") == 0) {
        if (!isOn(value) && !isOff(value)) { _err("value must be on or off"); return; }
        gSettings.debug.debugMode = isOn(value);
        menuSyncSettings();
        gSettings.save();
        _ok(gSettings.debug.debugMode ? "debug mode ON  (simulated temps active)"
                                       : "debug mode OFF (using real sensors)");

    // --- simulated chamber temp ---
    } else if (strcasecmp(key, "chamber") == 0) {
        float v = atof(value);
        if (v < 0.0f || v > 100.0f) { _err("range 0-100 C"); return; }
        gSettings.debug.debugChamberTemp = v;
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  chamber sim = %.1f C\n", v);

    // --- simulated bed temp ---
    } else if (strcasecmp(key, "bed") == 0) {
        float v = atof(value);
        if (v < 0.0f || v > 150.0f) { _err("range 0-150 C"); return; }
        gSettings.debug.debugBedTemp = v;
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  bed sim = %.1f C\n", v);

    // --- manual fan control ---
    } else if (strcasecmp(key, "manual") == 0) {
        if (!isOn(value) && !isOff(value)) { _err("value must be on or off"); return; }
        if (isOn(value)) {
            gSettings.debug.manualFanControl = true;
            gSettings.operatingMode = OperatingMode::Manual;
        } else {
            gSettings.debug.manualFanControl = false;
            gSettings.operatingMode = OperatingMode::Auto;
        }
        menuSyncSettings();
        gSettings.save();
        _ok(gSettings.debug.manualFanControl ? "manual fan control ON"
                                             : "manual fan control OFF");

    // --- manual heating fan ---
    } else if (strcasecmp(key, "heating") == 0) {
        int v = atoi(value);
        if (v < 0 || v > 100) { _err("range 0-100"); return; }
        gSettings.debug.manualHeatingFanSpeed = static_cast<uint8_t>(v);
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  manual heating = %d %%\n", v);

    // --- manual exhaust fan ---
    } else if (strcasecmp(key, "exhaust") == 0) {
        int v = atoi(value);
        if (v < 0 || v > 100) { _err("range 0-100"); return; }
        gSettings.debug.manualExhaustFanSpeed = static_cast<uint8_t>(v);
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  manual exhaust = %d %%\n", v);

    // --- manual recirc fan ---
    } else if (strcasecmp(key, "recirc") == 0) {
        int v = atoi(value);
        if (v < 0 || v > 100) { _err("range 0-100"); return; }
        gSettings.debug.manualRecircFanSpeed = static_cast<uint8_t>(v);
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  manual recirc = %d %%\n", v);

    // --- operating mode ---
    } else if (strcasecmp(key, "mode") == 0) {
        OperatingMode m;
        if      (strcasecmp(value, "auto")    == 0) m = OperatingMode::Auto;
        else if (strcasecmp(value, "heating") == 0) m = OperatingMode::Heat;
        else if (strcasecmp(value, "cooling") == 0) m = OperatingMode::Cool;
        else { _err("value must be auto, heating, or cooling"); return; }
        gSettings.operatingMode = m;
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  mode = %s\n", value);

    // --- mode decision time ---
    } else if (strcasecmp(key, "mdt") == 0) {
        int v = atoi(value);
        if (v < 1 || v > 60) { _err("range 1-60 minutes"); return; }
        gSettings.modeDecisionTimeMin = static_cast<uint16_t>(v);
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  mdt = %d min\n", v);

    // --- recirc start bed temp ---
    } else if (strcasecmp(key, "rfsbt") == 0) {
        int v = atoi(value);
        if (v < 20 || v > 100) { _err("range 20-100 C"); return; }
        gSettings.recircStartBedTemp = static_cast<uint8_t>(v);
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  rfsbt = %d C\n", v);

    // --- hot bed threshold ---
    } else if (strcasecmp(key, "threshold") == 0) {
        int v = atoi(value);
        if (v < 20 || v > 120) { _err("range 20-120 C"); return; }
        gSettings.hot.bedTempThreshold = static_cast<uint8_t>(v);
        menuSyncSettings();
        gSettings.save();
        Serial.printf("[Serial] OK  threshold = %d C\n", v);

    // --- fan presence: heating ---
    } else if (strcasecmp(key, "hfp") == 0) {
        if (!isOn(value) && !isOff(value)) { _err("value must be on or off"); return; }
        gSettings.debug.heatingFanPresent = isOn(value);
        applyFanPresence();
        applyFanPresenceToMenu();
        menuSyncSettings();
        gSettings.save();
        _ok(gSettings.debug.heatingFanPresent ? "heating fan present" : "heating fan absent");

    // --- fan presence: exhaust ---
    } else if (strcasecmp(key, "efp") == 0) {
        if (!isOn(value) && !isOff(value)) { _err("value must be on or off"); return; }
        gSettings.debug.exhaustFanPresent = isOn(value);
        applyFanPresence();
        applyFanPresenceToMenu();
        menuSyncSettings();
        gSettings.save();
        _ok(gSettings.debug.exhaustFanPresent ? "exhaust fan present" : "exhaust fan absent");

    // --- fan presence: recirc ---
    } else if (strcasecmp(key, "rfp") == 0) {
        if (!isOn(value) && !isOff(value)) { _err("value must be on or off"); return; }
        gSettings.debug.recircFanPresent = isOn(value);
        applyFanPresence();
        applyFanPresenceToMenu();
        menuSyncSettings();
        gSettings.save();
        _ok(gSettings.debug.recircFanPresent ? "recirc fan present" : "recirc fan absent");

    // --- chamber light ---
    } else if (strcasecmp(key, "light") == 0) {
        if (!isOn(value) && !isOff(value)) { _err("value must be on or off"); return; }
        gSettings.chamberLightOn = isOn(value);
        digitalWrite(PIN_CHAMBER_LIGHT, gSettings.chamberLightOn ? HIGH : LOW);
        menuSyncSettings();
        gSettings.save();
        _ok(gSettings.chamberLightOn ? "chamber light ON" : "chamber light OFF");

    } else {
        Serial.printf("[Serial] Unknown key '%s'. Type 'help'.\n", key);
    }
}

// ---------------------------------------------------------------------------
// log
// ---------------------------------------------------------------------------
void SerialInterface::_cmdLog(const char* sub, const char* arg) {
    if (!sub) { _err("usage: log list|fetch|delete"); return; }

    // ---- list ----
    if (strcasecmp(sub, "list") == 0) {
        const char* paths[] = { "/active.log", "/HOT.log", "/COLD.log" };
        Serial.println("-----------------------------------------------------");
        Serial.println(" Log files");
        Serial.println("-----------------------------------------------------");
        for (const char* p : paths) {
            if (!LittleFS.exists(p)) {
                Serial.printf(" %-12s: not found\n", p + 1);
                continue;
            }
            File f = LittleFS.open(p, "r");
            size_t sz   = f.size();
            int    rows = 0;
            while (f.available()) {
                String line = f.readStringUntil('\n');
                if (line.length() > 0 && line[0] != '#') rows++;
            }
            f.close();
            bool running = (strcmp(p, "/active.log") == 0);
            Serial.printf(" %-12s: %6d bytes  (%d rows%s)\n",
                          p + 1, (int)sz, rows,
                          running ? ", running" : "");
        }
        Serial.println("-----------------------------------------------------");
        return;
    }

    // ---- fetch / delete — need arg ----
    if (!arg) { _err("usage: log fetch|delete hot|cold|active"); return; }

    const char* path = nullptr;
    if      (strcasecmp(arg, "hot")    == 0) path = "/HOT.log";
    else if (strcasecmp(arg, "cold")   == 0) path = "/COLD.log";
    else if (strcasecmp(arg, "active") == 0) path = "/active.log";
    else { _err("argument must be hot, cold, or active"); return; }

    // ---- fetch ----
    if (strcasecmp(sub, "fetch") == 0) {
        if (!LittleFS.exists(path)) { _err("file not found"); return; }
        File f = LittleFS.open(path, "r");
        Serial.println("-----------------------------------------------------");
        while (f.available()) {
            Serial.println(f.readStringUntil('\n'));
        }
        f.close();
        Serial.println("-----------------------------------------------------");
        return;
    }

    // ---- delete ----
    if (strcasecmp(sub, "delete") == 0) {
        if (strcasecmp(arg, "active") == 0) {
            _err("cannot delete active log — end the print job first");
            return;
        }
        if (!LittleFS.exists(path)) { _err("file not found"); return; }
        LittleFS.remove(path);
        Serial.printf("[Serial] OK  deleted %s\n", path + 1);
        return;
    }

    _err("usage: log list|fetch|delete");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void SerialInterface::_ok(const char* msg) {
    if (msg) Serial.printf("[Serial] OK  %s\n", msg);
    else     Serial.println("[Serial] OK");
}

void SerialInterface::_err(const char* msg) {
    Serial.printf("[Serial] ERR %s\n", msg);
}
