#pragma once

// ---------------------------------------------------------------------------
// WifiManager.h
// NOTE: WiFi is not currently used in the application.
//       This class is reserved for future development (e.g. remote monitoring,
//       OTA updates, or a web configuration interface).
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "wifi_credentials.h"

class WifiManager {
public:
    void begin();
    bool isConnected() const;
    const char* getStatusText();

private:
    char _statusText[24] = "Not Connected";
};
