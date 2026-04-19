# Network Settings with On-Screen Keyboard — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace hardcoded WiFi credentials with user-editable SSID and password via the existing on-screen keyboard, stored in EEPROM.

**Architecture:** Add `NetworkSettings` (SSID + password char arrays) to the Settings struct and EEPROM layout. Create a "Network" submenu under Debug with two ActionMenuItems that launch ScreenKeyboard for editing, plus the existing WiFi IP display. WifiManager reads credentials from gSettings instead of compile-time defines. A `wifiReconnect()` function in main.cpp handles reconnection and web server startup after credential changes.

**Tech Stack:** ESP32 Arduino, TcMenu, EEPROM emulation, built-in WiFi/WebServer libraries

---

## File Structure

| File | Responsibility |
|------|---------------|
| `src/settings/Settings.h` | Add `NetworkSettings` struct, add `network` field to `Settings` |
| `src/settings/Settings.cpp` | Add `network` to `SettingsData`, defaults, bump magic |
| `src/menu/MenuSetup.h` | Add new menu item IDs (59, 60, 61) |
| `src/menu/MenuSetup.cpp` | Network submenu, ActionMenuItems, keyboard callbacks, move WiFi IP |
| `src/network/WifiManager.h` | Remove `wifi_credentials.h` include, add `reconnect()` |
| `src/network/WifiManager.cpp` | Read from `gSettings.network`, implement `reconnect()` |
| `src/main.cpp` | Add `wifiReconnect()` free function, `webServerStarted` flag |

---

### Task 1: Add NetworkSettings to Settings

**Files:**
- Modify: `src/settings/Settings.h`
- Modify: `src/settings/Settings.cpp`

- [ ] **Step 1: Add NetworkSettings struct and field to Settings.h**

In `src/settings/Settings.h`, add the struct before `class Settings` and the field inside the class:

```cpp
// After the DebugSettings struct (after line 50), add:

struct NetworkSettings {
    char ssid[33];       // max 32 chars + null (WiFi SSID spec limit)
    char password[65];   // max 64 chars + null (WPA2 passphrase limit)
};
```

Add `NetworkSettings network;` to the `Settings` class, after `DebugSettings debug;` (after line 71):

```cpp
    DebugSettings       debug;
    NetworkSettings     network;
```

Bump the magic constant (line 15):

```cpp
#define SETTINGS_MAGIC  0xFC05   // bump when Settings layout changes
```

- [ ] **Step 2: Add network to SettingsData and defaults in Settings.cpp**

In `src/settings/Settings.cpp`, add `NetworkSettings network;` to the `SettingsData` struct (after line 23):

```cpp
struct SettingsData {
    uint8_t       operatingMode;
    uint16_t      modeDecisionTimeMin;
    uint8_t       recircStartBedTemp;
    uint8_t       recircStartSpeed;
    bool          chamberLightOn;
    HotChamberSettings  hot;
    ColdChamberSettings cold;
    DebugSettings       debug;
    NetworkSettings     network;
};
```

Add defaults in `applyDefaults()` (after line 68, after `chamberLightOn = false;`):

```cpp
    network.ssid[0]     = '\0';
    network.password[0] = '\0';
```

Add network to the `load()` method (after line 92, after `debug = data.debug;`):

```cpp
    network               = data.network;
```

Add network to the `save()` method (after line 110, after `data.debug = debug;`):

```cpp
    data.network          = network;
```

- [ ] **Step 3: Build to verify compilation**

Run: `pio run 2>&1 | tail -5`
Expected: `[SUCCESS]`

- [ ] **Step 4: Commit**

```bash
git add src/settings/Settings.h src/settings/Settings.cpp
git commit -m "feat: add NetworkSettings to EEPROM (SSID + password)"
```

---

### Task 2: Update WifiManager to use settings

**Files:**
- Modify: `src/network/WifiManager.h`
- Modify: `src/network/WifiManager.cpp`

- [ ] **Step 1: Update WifiManager.h**

Remove the `wifi_credentials.h` include and add `reconnect()`:

Replace the entire file content with:

```cpp
#pragma once

#include <Arduino.h>

class WifiManager {
public:
    void begin();
    void reconnect();
    bool isConnected() const;
    const char* getStatusText();

private:
    char _statusText[24] = "Not Configured";
};
```

- [ ] **Step 2: Update WifiManager.cpp**

Replace the entire file content with:

```cpp
#include "WifiManager.h"
#include "../settings/Settings.h"
#include <WiFi.h>

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
```

- [ ] **Step 3: Build to verify compilation**

Run: `pio run 2>&1 | tail -5`
Expected: `[SUCCESS]`

- [ ] **Step 4: Commit**

```bash
git add src/network/WifiManager.h src/network/WifiManager.cpp
git commit -m "feat: WifiManager reads credentials from EEPROM settings"
```

---

### Task 3: Add wifiReconnect() to main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add webServerStarted flag and wifiReconnect function**

After the `static StatusWebServer webServer;` line (line 36), add:

```cpp
static bool webServerStarted = false;
```

After the `taskSerialStatus()` function (after line 150), add:

```cpp
// ============================================================
// wifiReconnect — called from menu after keyboard edits credentials
// ============================================================
void wifiReconnect() {
    wifiManager.reconnect();
    menuSetWifiIpStatus(wifiManager.getStatusText());
    if (wifiManager.isConnected() && !webServerStarted) {
        webServer.begin();
        taskManager.scheduleFixedRate(50,
                                      [] { webServer.handleClient(); },
                                      TIME_MILLIS);
        webServerStarted = true;
    }
    Serial.printf("[WiFi] Reconnect: %s\n", wifiManager.getStatusText());
}
```

- [ ] **Step 2: Set webServerStarted flag in existing boot flow**

In `setup()`, replace the two blocks that check `wifiManager.isConnected()` (lines 184-187 and lines 250-254). Change the first block:

```cpp
    // Start HTTP status server (only useful when WiFi is connected)
    if (wifiManager.isConnected()) {
        webServer.begin();
        webServerStarted = true;
    }
```

Change the second block (the handleClient scheduler near the end of setup):

```cpp
    // HTTP server — poll for incoming requests
    if (webServerStarted) {
        taskManager.scheduleFixedRate(50,
                                      [] { webServer.handleClient(); },
                                      TIME_MILLIS);
    }
```

- [ ] **Step 3: Build to verify compilation**

Run: `pio run 2>&1 | tail -5`
Expected: `[SUCCESS]`

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: add wifiReconnect() for runtime credential changes"
```

---

### Task 4: Add Network submenu to MenuSetup

**Files:**
- Modify: `src/menu/MenuSetup.h`
- Modify: `src/menu/MenuSetup.cpp`

- [ ] **Step 1: Add new menu IDs to MenuSetup.h**

In `src/menu/MenuSetup.h`, add these IDs to the `MenuItemId` enum (after `ID_DEBUG_LOG_INTERVAL = 58`, line 106):

```cpp
    ID_NET_SSID            = 59,
    ID_NET_PASSWORD        = 60,
    ID_NET_MENU            = 61,
```

Add a declaration for the reconnect function and the network edit callback (after the `void onSettingChanged(int id);` line, line 141):

```cpp
// Called from keyboard done callback to reconnect WiFi
extern void wifiReconnect();
```

- [ ] **Step 2: Add Network submenu items to MenuSetup.cpp**

This is the most involved step. The menu items are built bottom-up. The Network submenu goes between the Cooling PID submenu and the fan presence flags.

In `src/menu/MenuSetup.cpp`, add the ScreenKeyboard include at the top (after the existing includes, around line 7):

```cpp
#include "../ui/ScreenKeyboard.h"
```

Now restructure the WiFi IP and surrounding items. The current chain in the debug item list is:

```
menuDebugHeatingFan → menuDebugExhaustFan → menuDebugRecircFan →
menuDebugLogInterval → menuDebugChamberLight → menuWifiIp → nullptr
```

We need to:
1. Move `menuWifiIp` into the Network submenu
2. Create SSID and Password ActionMenuItems in the Network submenu
3. Insert the Network submenu into the debug chain

Replace the WiFi IP and Chamber Light section (lines 134-145) with:

```cpp
// ---- Network submenu items (bottom-up: last item first) ----

// WiFi IP (read-only, moved into Network submenu)
const AnyMenuInfo minfoWifiIp = {
    "WiFi IP", ID_WIFI_IP, 0xffff, 23, NO_CALLBACK
};
TextMenuItem menuWifiIp(&minfoWifiIp, "Not Configured", 24, nullptr, INFO_LOCATION_RAM);

// WiFi Password action item
const AnyMenuInfo minfoNetPassword = {
    "WiFi Password", ID_NET_PASSWORD, 0xffff, 0, NO_CALLBACK
};
ActionMenuItem menuNetPassword(&minfoNetPassword, &menuWifiIp, INFO_LOCATION_RAM);

// WiFi SSID action item
const AnyMenuInfo minfoNetSsid = {
    "WiFi SSID", ID_NET_SSID, 0xffff, 0, NO_CALLBACK
};
ActionMenuItem menuNetSsid(&minfoNetSsid, &menuNetPassword, INFO_LOCATION_RAM);

// Network submenu
const SubMenuInfo minfoNetwork = { "Network", ID_NET_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackNetwork(&minfoNetwork, &menuNetSsid, INFO_LOCATION_RAM);
SubMenuItem menuNetwork(&minfoNetwork, &menuBackNetwork, nullptr, INFO_LOCATION_RAM);

// Chamber Light Present YES/NO
const BooleanMenuInfo minfoDebugChamberLight = {
    "Chamber Light Present", ID_DEBUG_CHAMBER_LIGHT, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuDebugChamberLight(&minfoDebugChamberLight, true, nullptr, INFO_LOCATION_RAM);
```

Note: `menuDebugChamberLight` now points to `nullptr` instead of `&menuWifiIp` (WiFi IP moved into Network submenu).

Update `menuDebugLogInterval` to point to `&menuDebugChamberLight` (this is unchanged — line 151 already does this).

- [ ] **Step 3: Insert Network submenu into the debug chain**

The debug submenu chain needs Network inserted. In `menuSetup()` (around line 448-450), after the existing fixup lines, add:

```cpp
    // Insert Network submenu into debug chain: PID → Network → HeatingFan
    menuDebugPid.setNext(&menuNetwork);
    menuNetwork.setNext(&menuDebugHeatingFan);
```

And remove or comment out the existing line:
```cpp
    // OLD: menuDebugPid.setNext(&menuDebugHeatingFan);
```

So the block becomes:

```cpp
    // Finalize debug submenu ordering after all items are constructed.
    menuBackDebugPid.setNext(&menuDebugKp);
    menuDebugPid.setNext(&menuNetwork);
    menuNetwork.setNext(&menuDebugHeatingFan);
    menuMgr.addChangeNotification(&rootSelectionGuard);
```

- [ ] **Step 4: Add keyboard callback and static edit buffers**

After the `_loadSettingsToMenu()` function (after line 504), add:

```cpp
// ===========================================================================
// Network editing via ScreenKeyboard
// ===========================================================================

// Static buffers for keyboard editing (must outlive the keyboard session)
static char _netEditBuf[65];       // large enough for password (64 + null)
static bool _editingSsid = false;  // true = editing SSID, false = editing password

static void _onNetworkEditDone(bool accepted) {
    if (accepted) {
        if (_editingSsid) {
            strncpy(gSettings.network.ssid, _netEditBuf, sizeof(gSettings.network.ssid));
            gSettings.network.ssid[sizeof(gSettings.network.ssid) - 1] = '\0';
            Serial.printf("[Menu] WiFi SSID set to: %s\n", gSettings.network.ssid);
        } else {
            strncpy(gSettings.network.password, _netEditBuf, sizeof(gSettings.network.password));
            gSettings.network.password[sizeof(gSettings.network.password) - 1] = '\0';
            Serial.println("[Menu] WiFi password updated");
        }
        gSettings.save();
        wifiReconnect();
    }
}

void onNetworkEdit(int id) {
    if (id == ID_NET_SSID) {
        _editingSsid = true;
        strncpy(_netEditBuf, gSettings.network.ssid, sizeof(_netEditBuf));
        _netEditBuf[sizeof(_netEditBuf) - 1] = '\0';
        ScreenKeyboard::activate("WiFi SSID:", _netEditBuf, 33, _onNetworkEditDone);
    } else if (id == ID_NET_PASSWORD) {
        _editingSsid = false;
        _netEditBuf[0] = '\0';   // never pre-fill password (security)
        ScreenKeyboard::activate("WiFi Password:", _netEditBuf, 65, _onNetworkEditDone);
    }
}
```

- [ ] **Step 5: Wire ActionMenuItems to the callback**

The `AnyMenuInfo` structs for SSID and Password currently have `NO_CALLBACK`. Change them to use `onNetworkEdit`:

```cpp
const AnyMenuInfo minfoNetPassword = {
    "WiFi Password", ID_NET_PASSWORD, 0xffff, 0, onNetworkEdit
};

const AnyMenuInfo minfoNetSsid = {
    "WiFi SSID", ID_NET_SSID, 0xffff, 0, onNetworkEdit
};
```

Add the forward declaration of `onNetworkEdit` near the top of the file (after line 33, near the other forward declarations):

```cpp
static void _onNetworkEditDone(bool accepted);
void onNetworkEdit(int id);
```

- [ ] **Step 6: Build to verify compilation**

Run: `pio run 2>&1 | tail -5`
Expected: `[SUCCESS]`

- [ ] **Step 7: Commit**

```bash
git add src/menu/MenuSetup.h src/menu/MenuSetup.cpp
git commit -m "feat: add Network submenu with keyboard-editable SSID/password"
```

---

### Task 5: Upload and test on hardware

**Files:**
- No changes — verification only

- [ ] **Step 1: Upload firmware**

Run: `pio run -t upload 2>&1 | tail -5`
Expected: `[SUCCESS]`

- [ ] **Step 2: Read serial boot output**

```bash
python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=1)
end = time.time() + 8
while time.time() < end:
    line = ser.readline().decode('utf-8', errors='replace').strip()
    if line:
        print(line)
ser.close()
"
```

Expected output includes:
- `[WiFi] No SSID configured — skipping` (because EEPROM magic changed, defaults applied)
- `[Boot] No saved settings — writing defaults`

- [ ] **Step 3: Navigate to Network submenu on device**

Using the encoder, navigate to: Settings → Debug Settings → Network

Verify three items visible:
1. WiFi SSID
2. WiFi Password
3. WiFi IP (showing "Not Configured")

- [ ] **Step 4: Edit SSID via on-screen keyboard**

Select "WiFi SSID" — the keyboard should take over the screen. Type your SSID using the T9 keys, press OK.

Verify serial output shows:
```
[Menu] WiFi SSID set to: <your-ssid>
[WiFi] Connecting to <your-ssid>
```

- [ ] **Step 5: Edit password and verify connection**

Select "WiFi Password" — keyboard appears with empty buffer (not pre-filled). Type password, press OK.

Verify serial output shows:
```
[Menu] WiFi password updated
[WiFi] Connecting to <your-ssid>
[WiFi] WiFi success: <ip-address>
[Web] HTTP server started on port 80
```

- [ ] **Step 6: Verify web server works**

```bash
curl -s http://<ip-address>/ | head -5
```

Expected: the plain-text status page.

- [ ] **Step 7: Verify persistence across reboot**

Power-cycle the device (unplug USB and replug). Read serial output:

```bash
python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=1)
end = time.time() + 15
while time.time() < end:
    line = ser.readline().decode('utf-8', errors='replace').strip()
    if line:
        print(line)
ser.close()
"
```

Expected:
```
[Boot] Settings loaded from EEPROM
[WiFi] Connecting to <your-ssid>
[WiFi] WiFi success: <ip-address>
[Web] HTTP server started on port 80
```

---

### Task 6: Update documentation

**Files:**
- Modify: `PROJECT.md`
- Modify: `README.md`

- [ ] **Step 1: Update PROJECT.md**

In the "Current Status — Working" section, update the WiFi bullet:

```
  - Wi-Fi credentials are configured via on-screen keyboard (Settings →
    Debug → Network) and persisted to EEPROM. The hardcoded credentials
    file is no longer required.
```

In the "Not started" section, remove the line about persistent Wi-Fi configuration (it's now done).

- [ ] **Step 2: Update README.md**

In the "Getting Started" section, replace step 2 ("Create WiFi credentials file") with:

```markdown
### 2. Configure WiFi (on device)

WiFi credentials are entered on the device itself using the on-screen
keyboard. Navigate to **Settings → Debug → Network** and edit the SSID
and password. Credentials are saved to flash and persist across reboots.

> A `wifi_credentials.h` file is no longer required. If you previously
> used one, the device will prompt for new credentials after this update
> (EEPROM layout has changed).
```

- [ ] **Step 3: Commit**

```bash
git add PROJECT.md README.md
git commit -m "docs: update WiFi setup instructions for on-screen keyboard"
```
