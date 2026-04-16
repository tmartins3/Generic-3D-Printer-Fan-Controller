#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// WifiManager.h
// Manages WiFi connection using credentials stored in gSettings.network.
// If no SSID is configured, WiFi is skipped entirely.
// ---------------------------------------------------------------------------

class WifiManager {
public:
    void begin();
    void reconnect();
    bool isConnected() const;
    const char* getStatusText();

private:
    char _statusText[24] = "Not Configured";
};
