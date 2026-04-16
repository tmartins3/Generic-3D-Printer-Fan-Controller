#include "WifiManager.h"
#include "../settings/Settings.h"
#include <WiFi.h>

// ---------------------------------------------------------------------------
// WifiManager.cpp
// ---------------------------------------------------------------------------

void WifiManager::begin() {
    // If no SSID configured, skip WiFi entirely
    if (gSettings.network.ssid[0] == '\0') {
        strncpy(_statusText, "Not Configured", sizeof(_statusText));
        _statusText[sizeof(_statusText) - 1] = '\0';
        Serial.println("[WiFi] No SSID configured — skipping");
        return;
    }

    strncpy(_statusText, "Not Connected", sizeof(_statusText));
    _statusText[sizeof(_statusText) - 1] = '\0';

    WiFi.mode(WIFI_STA);
    if (gSettings.network.password[0] == '\0') {
        WiFi.begin(gSettings.network.ssid);
    } else {
        WiFi.begin(gSettings.network.ssid, gSettings.network.password);
    }

    Serial.printf("[WiFi] Connecting to %s\n", gSettings.network.ssid);

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

void WifiManager::reconnect() {
    WiFi.disconnect(true, true);
    begin();
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

const char* WifiManager::getStatusText() {
    if (gSettings.network.ssid[0] == '\0') {
        strncpy(_statusText, "Not Configured", sizeof(_statusText));
        _statusText[sizeof(_statusText) - 1] = '\0';
    } else if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        ip.toCharArray(_statusText, sizeof(_statusText));
    } else {
        strncpy(_statusText, "Not Connected", sizeof(_statusText));
        _statusText[sizeof(_statusText) - 1] = '\0';
    }
    return _statusText;
}
