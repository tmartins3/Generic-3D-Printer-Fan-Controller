# Network Settings with On-Screen Keyboard — Design Spec

**Date:** 2026-04-16
**Project:** FanController2 (ESP32 3D printer enclosure controller)

---

## Overview

Replace the hardcoded WiFi credentials with user-editable SSID and
password fields stored in EEPROM. The existing on-screen keyboard
(`ScreenKeyboard`) provides the input UI. A new "Network" submenu under
Debug groups the WiFi-related items together.

---

## Settings Changes

### New struct: `NetworkSettings`

```cpp
struct NetworkSettings {
    char ssid[33];       // max 32 chars + null (WiFi SSID spec limit)
    char password[65];   // max 64 chars + null (WPA2 passphrase limit)
};
```

### Integration into `Settings`

- Add `NetworkSettings network;` field to the `Settings` class
- Add `NetworkSettings network;` field to the `SettingsData` POD struct
- Default values: empty strings (`ssid[0] = '\0'`, `password[0] = '\0'`)
- Bump `SETTINGS_MAGIC` to `0xFC05`
- `load()` and `save()` already work via `SettingsData` memcpy — no
  additional serialisation code needed beyond adding the field

---

## Menu Changes

### New IDs

| ID | Name | Purpose |
|----|------|---------|
| 59 | `ID_NET_SSID` | WiFi SSID (ActionMenuItem) |
| 60 | `ID_NET_PASSWORD` | WiFi Password (ActionMenuItem) |
| 61 | `ID_NET_MENU` | Network submenu |

### New submenu: "Network"

Located inside the Debug submenu. Contains:

1. **WiFi SSID** — `ActionMenuItem`, ID 59. On select: opens
   `ScreenKeyboard` with prompt "WiFi SSID:", editing
   `gSettings.network.ssid` (buffer size 33).
2. **WiFi Password** — `ActionMenuItem`, ID 60. On select: opens
   `ScreenKeyboard` with prompt "WiFi Password:", editing
   `gSettings.network.password` (buffer size 65). The keyboard edit
   buffer is pre-filled empty (never pre-filled with the current
   password). The menu item displays "Set" or "Not Set" — never the
   actual password.
3. **WiFi IP** — `TextMenuItem`, ID 7 (existing). Read-only. Moved from
   its current position in the Debug item chain into this submenu.

### Updated Debug menu chain

```
Debug Settings
├── Manual Sensor Control  (submenu)
├── Manual Fan Control     (submenu)
├── Cooling PID            (submenu)
├── Network                (submenu)  ← NEW
│   ├── WiFi SSID          (action)
│   ├── WiFi Password      (action)
│   └── WiFi IP            (text, RO)
├── Heating Fan Present    (boolean)
├── Exhaust Fan Present    (boolean)
├── Recirc Fan Present     (boolean)
├── Chamber Light Present  (boolean)
├── Logging Interval       (analog)
```

### ActionMenuItem callback flow

When the user selects SSID or Password:

1. Menu callback `onNetworkEdit(int id)` fires
2. Copy `gSettings.network.ssid` (or `.password`) into a static edit
   buffer
3. Call `ScreenKeyboard::activate(prompt, editBuf, bufSize, onDone)`
4. Keyboard takes over full screen
5. On OK (`onDone(true)`):
   - Copy edit buffer back to `gSettings.network.ssid` (or `.password`)
   - Call `gSettings.save()`
   - Call `wifiReconnect()` (free function in main.cpp)
   - Update WiFi IP menu text
6. On ESC (`onDone(false)`): no changes

---

## WifiManager Changes

### Remove hardcoded credentials

- Remove `#include "wifi_credentials.h"` from `WifiManager.h`
- `begin()` reads `gSettings.network.ssid` and
  `gSettings.network.password` instead of `WIFI_SSID`/`WIFI_PASSWORD`
- If `gSettings.network.ssid[0] == '\0'`, skip connection and set
  status text to "Not Configured"

### New method: `reconnect()`

```cpp
void WifiManager::reconnect() {
    WiFi.disconnect(true, true);
    begin();   // re-runs connection with current gSettings.network
}
```

Called from `main.cpp`'s `wifiReconnect()` free function after the
keyboard saves new credentials.

### Web server restart

`wifiReconnect()` in `main.cpp` also starts the web server and its
`handleClient` task if WiFi connects successfully and the server isn't
already running.

---

## main.cpp Changes

### New free function: `wifiReconnect()`

```cpp
void wifiReconnect() {
    wifiManager.reconnect();
    menuSetWifiIpStatus(wifiManager.getStatusText());
    if (wifiManager.isConnected() && !webServerStarted) {
        webServer.begin();
        taskManager.scheduleFixedRate(50,
            [] { webServer.handleClient(); }, TIME_MILLIS);
        webServerStarted = true;
    }
}
```

Declared as `extern "C"` or just `extern void wifiReconnect();` in a
header or at the top of `MenuSetup.cpp` so the keyboard callback can
call it.

### Static flag: `webServerStarted`

Prevents scheduling duplicate `handleClient` tasks on repeated
reconnects. Set to `true` after the first successful web server start
(either at boot or after keyboard-triggered reconnect).

---

## Files Changed

| File | Change |
|------|--------|
| `src/settings/Settings.h` | Add `NetworkSettings` struct, add `network` field, bump `SETTINGS_MAGIC` to `0xFC05` |
| `src/settings/Settings.cpp` | Add `network` to `SettingsData`, set defaults (empty strings), load/save via existing memcpy pattern |
| `src/menu/MenuSetup.h` | Add `ID_NET_SSID = 59`, `ID_NET_PASSWORD = 60`, `ID_NET_MENU = 61` |
| `src/menu/MenuSetup.cpp` | Add Network submenu with ActionMenuItems, keyboard callbacks, move WiFi IP into Network submenu |
| `src/network/WifiManager.h` | Remove `wifi_credentials.h` include, add `reconnect()` declaration |
| `src/network/WifiManager.cpp` | Read from `gSettings.network`, implement `reconnect()`, handle empty SSID |
| `src/main.cpp` | Add `wifiReconnect()` free function, `webServerStarted` flag, adjust boot flow |

No new files. `ScreenKeyboard` is already implemented.

---

## Error Handling

| Condition | Behaviour |
|-----------|-----------|
| SSID is empty | WiFi skipped, IP shows "Not Configured", web server not started |
| WiFi connection fails after credential change | IP shows "Not Connected", web server not started, user can re-edit |
| Password is empty | Attempt open network connection (WiFi.begin with SSID only) |
| EEPROM magic mismatch after adding NetworkSettings | Defaults applied — SSID/password empty, user must enter via keyboard |

---

## Capacity

`NetworkSettings` adds 98 bytes to EEPROM usage (33 + 65). Total
`SettingsData` size remains well within the ESP32 EEPROM emulation
limit of 4096 bytes.

## Security
The wifi password is not visible in the menu once input. After input it is saved into EEPROM and read from there when needed.

