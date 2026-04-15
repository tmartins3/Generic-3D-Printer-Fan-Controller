#pragma once

#include <Arduino.h>

class WifiManager {
public:
    void begin();
    bool isConnected() const;
    const char* getStatusText();

private:
    static constexpr const char* WIFI_SSID = "God24";
    static constexpr const char* WIFI_PASSWORD = "tomasonkovajatka";

    char _statusText[24] = "Not Connected";
};
