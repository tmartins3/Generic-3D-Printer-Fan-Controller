#include "WifiManager.h"
#include "../settings/Settings.h"
#include <WiFi.h>

// ---------------------------------------------------------------------------
// WifiManager.cpp
// Non-blocking WiFi connection management.
// ---------------------------------------------------------------------------

static const unsigned long CONNECT_TIMEOUT_MS = 10000;

void WifiManager::begin() {
    // If no SSID configured, skip WiFi entirely
    if (gSettings.network.ssid[0] == '\0') {
        strncpy(_statusText, "Not Configured", sizeof(_statusText));
        _statusText[sizeof(_statusText) - 1] = '\0';
        _connecting = false;
        Serial.println("[WiFi] No SSID configured — skipping");
        return;
    }

    strncpy(_statusText, "Connecting...", sizeof(_statusText));
    _statusText[sizeof(_statusText) - 1] = '\0';

    WiFi.mode(WIFI_STA);
    if (gSettings.network.password[0] == '\0') {
        WiFi.begin(gSettings.network.ssid);
    } else {
        WiFi.begin(gSettings.network.ssid, gSettings.network.password);
    }

    _connectStartMs = millis();
    _connecting = true;
    Serial.printf("[WiFi] Connecting to %s (non-blocking)\n", gSettings.network.ssid);
}

bool WifiManager::poll() {
    if (!_connecting) return false;

    if (WiFi.status() == WL_CONNECTED) {
        _connecting = false;
        String ip = WiFi.localIP().toString();
        ip.toCharArray(_statusText, sizeof(_statusText));
        Serial.printf("[WiFi] WiFi success: %s\n", _statusText);
        return true;   // signal: just connected
    }

    if ((millis() - _connectStartMs) >= CONNECT_TIMEOUT_MS) {
        _connecting = false;
        strncpy(_statusText, "Not Connected", sizeof(_statusText));
        _statusText[sizeof(_statusText) - 1] = '\0';
        Serial.println("[WiFi] Connection timed out");
        WiFi.disconnect(true, true);
    }

    return false;
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
    } else if (!_connecting) {
        strncpy(_statusText, "Not Connected", sizeof(_statusText));
        _statusText[sizeof(_statusText) - 1] = '\0';
    }
    // If _connecting, keep "Connecting..." text
    return _statusText;
}
