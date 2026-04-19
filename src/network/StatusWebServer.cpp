#include "StatusWebServer.h"
#include "../settings/Settings.h"
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
    StringPrint sp(out);
    gSettings.dump(sp);
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
