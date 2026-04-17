#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// StatusWebServer.h
// Minimal HTTP server that returns a single plain-text page containing
// the current settings/state and the two log files (HOT.log, COLD.log).
// Requires WiFi to be connected before calling begin().
// ---------------------------------------------------------------------------

class StatusWebServer {
public:
    void begin();
    void handleClient();

private:
    void _handleRoot();
    void _appendSettings(String& out);
    void _appendLogFile(String& out, const char* path, const char* label);
};
