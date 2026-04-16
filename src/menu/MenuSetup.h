#pragma once

// ---------------------------------------------------------------------------
// MenuSetup.h
// TcMenu menu structure for FanController2, defined entirely in C++ code.
// The plugin files tcMenuAdaFruitGfx.h/.cpp (copied from TcMenu examples)
// provide AdafruitDrawable and the 2bpp canvas rendering for the ST7735s.
//
// Menu tree:
//   Root ("FanController")
//   ├── Mode         [enum RO]   IDLE / RECIRC / HEAT / COOL
//   ├── Bed Temp     [analog RO] °C
//   ├── Chamber      [analog RO] °C
//   ├── Heating Fan  [analog RO] %
//   ├── Recirc Fan   [analog RO] %
//   ├── Exhaust Fan  [analog RO] %
//   └── Settings     [submenu]
//       ├── Op Mode        [enum]   AUTO / HEAT / COOL
//       ├── Decision Time  [analog] min
//       ├── Recirc Temp    [analog] °C
//       ├── Recirc Speed   [analog] %
//       ├── Hot Chamber    [submenu]
//       │   ├── Heating Fan Speed [analog] %
//       │   ├── Recirc Speed  [analog] %
//       │   ├── Exhaust Spd   [analog] %
//       │   └── Bed Threshold [analog] °C
//       ├── Cold Chamber   [submenu]
//       │   ├── Exhaust Max   [analog] %
//       │   ├── Exhaust Min   [analog] %
//       │   ├── Recirc Speed  [analog] %
//       │   ├── Max Temp      [analog] °C
//       │   ├── PID Kp        [analog] x.xx
//       │   ├── PID Ki        [analog] x.xx
//       │   └── PID Kd        [analog] x.xx
//       └── Debug          [submenu]
//           ├── Debug Mode    [boolean]
//           ├── Chamber Temp  [analog] °C
//           └── Bed Temp      [analog] °C
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <tcMenu.h>
#include <MenuItems.h>
#include <RuntimeMenuItem.h>
#include "tcMenuAdaFruitGfx.h"
#include <graphics/GraphicsDeviceRenderer.h>
#include <graphics/TcThemeBuilder.h>

using namespace tcgfx;

// ---------------------------------------------------------------------------
// Application info required by GraphicsDeviceRenderer
// ---------------------------------------------------------------------------
extern const ConnectorLocalInfo applicationInfo;

// ---------------------------------------------------------------------------
// Renderer and drawable — defined in MenuSetup.cpp, used by TcMenu globally
// ---------------------------------------------------------------------------
extern GraphicsDeviceRenderer renderer;

// ---------------------------------------------------------------------------
// Menu item ID constants — used to identify items in the callback
// ---------------------------------------------------------------------------
enum MenuItemId : menuid_t {
    ID_MODE          = 1,
    ID_BED_TEMP      = 2,
    ID_CHAMBER_TEMP  = 3,
    ID_HEATING_FAN   = 4,
    ID_RECIRC_FAN    = 5,
    ID_EXHAUST_FAN   = 6,
    ID_WIFI_IP          = 7,
    ID_CHAMBER_LIGHT    = 8,
    ID_SETTINGS         = 10,
    ID_OP_MODE       = 11,
    ID_DECISION_TIME = 12,
    ID_RECIRC_TEMP   = 13,
    ID_RECIRC_SPEED  = 14,
    ID_HOT           = 20,
    ID_HOT_HEATING   = 21,
    ID_HOT_RECIRC    = 22,
    ID_HOT_EXHAUST   = 23,
    ID_HOT_THRESHOLD = 24,
    ID_COLD          = 30,
    ID_COLD_MAX      = 31,
    ID_COLD_MIN      = 32,
    ID_COLD_RECIRC   = 33,
    ID_COLD_TEMP     = 34,
    ID_DEBUG         = 40,
    ID_DEBUG_MODE    = 41,
    ID_DEBUG_CHAMBER = 42,
    ID_DEBUG_BED     = 43,
    ID_DEBUG_KP      = 44,
    ID_DEBUG_KI      = 45,
    ID_DEBUG_KD      = 46,
    ID_DEBUG_PID_MENU      = 47,
    ID_DEBUG_MANUAL_MENU   = 48,
    ID_DEBUG_MANUAL_ENABLE = 49,
    ID_DEBUG_MANUAL_HEATING = 50,
    ID_DEBUG_MANUAL_EXHAUST = 51,
    ID_DEBUG_MANUAL_RECIRC = 52,
    ID_DEBUG_HEATING_FAN   = 53,
    ID_DEBUG_EXHAUST_FAN   = 54,
    ID_DEBUG_RECIRC_FAN    = 55,
    ID_DEBUG_SENSOR_MENU   = 56,
    ID_DEBUG_CHAMBER_LIGHT = 57,
    ID_DEBUG_LOG_INTERVAL  = 58,
    ID_NET_SSID            = 59,
    ID_NET_PASSWORD        = 60,
    ID_NET_MENU            = 61,
};

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

// Call once from setup(). Initialises display, encoder, renderer, menu tree.
void menuSetup();

// Update the read-only status items from the current system state.
// Call from a periodic task (e.g. every 500 ms).
void menuUpdateStatus(uint8_t  stateModeIndex,   // 0=IDLE 1=RECIRC 2=HEAT 3=COOL
                      float    bedTempC,
                      float    chamberTempC,
                      uint8_t  heatingFanPct,
                      uint8_t  recircFanPct,
                      uint8_t  exhaustFanPct);

void menuUpdateFooterStatus(uint8_t stateModeIndex, float chamberTempC);

void menuSetWifiIpStatus(const char* wifiStatusText);
uint8_t menuGetConfiguredOperatingModeIndex();
void applyFanPresenceToMenu();

// Re-sync all gSettings values back into the menu items.
// Call this after changing gSettings from outside the menu (e.g. serial interface).
void menuSyncSettings();

// Returns a reference to the raw Adafruit_GFX display object.
// Used by ScreenKeyboard to draw directly on the screen.
Adafruit_GFX& menuGetDisplay();

// Callback fired by TcMenu when the user confirms a setting change.
void onSettingChanged(int id);

// Called from keyboard done callback to reconnect WiFi
extern void wifiReconnect();
