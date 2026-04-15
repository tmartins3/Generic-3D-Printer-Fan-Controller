#include "WifiManager.h"

#include <WiFi.h>

void WifiManager::begin() {
    strncpy(_statusText, "Not Connected", sizeof(_statusText));
    _statusText[sizeof(_statusText) - 1] = 0;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 10000UL) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        ip.toCharArray(_statusText, sizeof(_statusText));
        Serial.printf("[WiFi] WiFi success: %s\n", _statusText);
    } else {
        Serial.println("[WiFi] Not Connected");
        WiFi.disconnect(true, true);
    }
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

const char* WifiManager::getStatusText() {
    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        ip.toCharArray(_statusText, sizeof(_statusText));
    } else {
        strncpy(_statusText, "Not Connected", sizeof(_statusText));
        _statusText[sizeof(_statusText) - 1] = 0;
    }
    return _statusText;
}
