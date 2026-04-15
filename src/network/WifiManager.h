#pragma once

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
