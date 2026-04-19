#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// WifiManager.h
// Manages WiFi connection using credentials stored in gSettings.network.
// If no SSID is configured, WiFi is skipped entirely.
//
// Connection is non-blocking: begin() starts the connection attempt and
// returns immediately. Call poll() periodically (e.g. every 500 ms) to
// check progress and update status text.
// ---------------------------------------------------------------------------

class WifiManager {
public:
    // Start a connection attempt (non-blocking). Returns immediately.
    void begin();

    // Check connection progress. Call periodically from a TaskManager task.
    // Returns true the first time a connection succeeds (useful for
    // starting the web server on connect).
    bool poll();

    // Disconnect and start a new connection attempt (non-blocking).
    void reconnect();

    bool isConnected() const;
    const char* getStatusText();

private:
    char _statusText[24] = "Not Configured";
    bool _connecting = false;
    unsigned long _connectStartMs = 0;
};
