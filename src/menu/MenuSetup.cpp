#include "MenuSetup.h"
#include "../../include/Config.h"
#include "../settings/Settings.h"

#include <SPI.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "../ui/ScreenKeyboard.h"

#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7735
#include <Adafruit_ST7735.h>
#elif TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
#include <Adafruit_ST7789.h>
#else
#error Unsupported TFT_DRIVER_TYPE in Config.h
#endif

// ---------------------------------------------------------------------------
// MenuSetup.cpp
// Menu structure follows the same pattern as TcMenu's generated _menu.cpp.
// Items are built bottom-up so each constructor can reference already-declared
// items via their address.
//
// AnalogMenuInfo field order:
//   { "name", id, eepromAddr, maxVal, callback, offset, divisor, "unit" }
//
// PID gains are stored as integer * 100 (range 0–1000 represents 0.00–10.00).
// ---------------------------------------------------------------------------

#define PID_DIV 100    // divisor that gives 2 decimal places in the menu
#define PID_MAX 1000   // max stored value (= 10.00)

// Forward declaration — defined after the menu items below
static void _loadSettingsToMenu();
static const char* _modeShortLabel(uint8_t stateModeIndex);
static void _drawFooterStatus(uint8_t stateModeIndex, float chamberTempC);
static bool _isTopLevelStatusItem(MenuItem* item);
static void _onNetworkEditDone(bool accepted);
void onNetworkEdit(int id);

extern EnumMenuItem menuMode;
extern AnalogMenuItem menuBedTemp;
extern AnalogMenuItem menuChamberTemp;
extern AnalogMenuItem menuHeatingFan;
extern AnalogMenuItem menuRecircFan;
extern AnalogMenuItem menuExhaustFan;
extern TextMenuItem menuWifiIp;
extern SubMenuItem menuSettings;

// ---------------------------------------------------------------------------
// Application info (required by GraphicsDeviceRenderer)
// ---------------------------------------------------------------------------
const ConnectorLocalInfo applicationInfo = { "FanController", "fan-ctrl-esp32-001" };

class RootSelectionGuard : public MenuManagerObserver {
public:
    void structureHasChanged() override {}
    bool menuEditStarting(MenuItem* item) override {
        // Block TcMenu's built-in text editor for items that use our keyboard
        if (item->getId() == ID_NET_SSID || item->getId() == ID_NET_PASSWORD) {
            onNetworkEdit(item->getId());
            return false;   // prevent TcMenu's editor
        }
        return true;
    }
    void menuEditEnded(MenuItem* item) override {}

    void activeItemHasChanged(MenuItem* newActive) override {
        if (_redirecting) return;
        if (menuMgr.getCurrentMenu() != &menuMode) return;
        if (!_isTopLevelStatusItem(newActive)) return;

        _redirecting = true;
        menuMgr.setItemActive(&menuSettings);
        _redirecting = false;
    }

private:
    bool _redirecting = false;
};

static RootSelectionGuard rootSelectionGuard;

class ViewportAdafruitDrawable : public AdafruitDrawable {
public:
    ViewportAdafruitDrawable(Adafruit_GFX* graphics, int16_t viewportHeight, int spriteHeight = 0)
        : AdafruitDrawable(graphics, spriteHeight), _viewportHeight(viewportHeight) {}

    Coord getDisplayDimensions() override {
        auto dims = AdafruitDrawable::getDisplayDimensions();
        return Coord(dims.x, _viewportHeight);
    }

private:
    int16_t _viewportHeight;
};

// ---------------------------------------------------------------------------
// Display + renderer (globals so TcMenu can access renderer via extern)
// ---------------------------------------------------------------------------
#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7735
static Adafruit_ST7735 gfx(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RES);
#elif TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
static Adafruit_ST7789 gfx(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RES);
#endif

static ViewportAdafruitDrawable gfxDrawable(&gfx, TFT_MENU_HEIGHT, 40);
GraphicsDeviceRenderer renderer(30, applicationInfo.name, &gfxDrawable);

// ===========================================================================
// DEBUG submenu  (build bottom-up: last item first, back item last)
// ===========================================================================

// PID Kd  [0.00–10.00, stored as 0–1000] — lives inside Exhaust Fan submenu
const AnalogMenuInfo minfoDebugKd = {
    "PID Kd", ID_DEBUG_KD, 0xffff, PID_MAX, onSettingChanged, 0, PID_DIV, ""
};
AnalogMenuItem menuDebugKd(&minfoDebugKd, 100, nullptr, INFO_LOCATION_RAM);

// ---- Shared fan type enum strings (0=2PIN, 1=3PIN, 2=4PIN) ----
const char fanTypeStr0[] = "2PIN";
const char fanTypeStr1[] = "3PIN";
const char fanTypeStr2[] = "4PIN";
const char* const fanTypeStrings[] = { fanTypeStr0, fanTypeStr1, fanTypeStr2 };

// ---- Heating Fan submenu ----

const AnalogMenuInfo minfoDebugManualHeating = {
    "Manual Speed", ID_DEBUG_MANUAL_HEATING, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuDebugManualHeating(&minfoDebugManualHeating, 0, nullptr, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoInvertPwmHeating = {
    "Invert PWM", ID_INVERT_PWM_HEATING, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuInvertPwmHeating(&minfoInvertPwmHeating, false, &menuDebugManualHeating, INFO_LOCATION_RAM);

const EnumMenuInfo minfoFanTypeHeating = {
    "Fan Type", ID_FAN_TYPE_HEATING, 0xffff, 2, onSettingChanged, fanTypeStrings
};
EnumMenuItem menuFanTypeHeating(&minfoFanTypeHeating, 2, &menuInvertPwmHeating, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoDebugHeatingFan = {
    "Fan Present", ID_DEBUG_HEATING_FAN, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuDebugHeatingFan(&minfoDebugHeatingFan, true, &menuFanTypeHeating, INFO_LOCATION_RAM);

const SubMenuInfo minfoFanHeating = { "Heating Fan Settings", ID_FAN_HEATING_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackFanHeating(&minfoFanHeating, &menuDebugHeatingFan, INFO_LOCATION_RAM);
SubMenuItem menuFanHeating(&minfoFanHeating, &menuBackFanHeating, nullptr, INFO_LOCATION_RAM);

// ---- Exhaust Fan submenu ----

const AnalogMenuInfo minfoDebugManualExhaust = {
    "Manual Speed", ID_DEBUG_MANUAL_EXHAUST, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuDebugManualExhaust(&minfoDebugManualExhaust, 0, &menuDebugKd, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoInvertPwmExhaust = {
    "Invert PWM", ID_INVERT_PWM_EXHAUST, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuInvertPwmExhaust(&minfoInvertPwmExhaust, false, &menuDebugManualExhaust, INFO_LOCATION_RAM);

const EnumMenuInfo minfoFanTypeExhaust = {
    "Fan Type", ID_FAN_TYPE_EXHAUST, 0xffff, 2, onSettingChanged, fanTypeStrings
};
EnumMenuItem menuFanTypeExhaust(&minfoFanTypeExhaust, 2, &menuInvertPwmExhaust, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoDebugExhaustFan = {
    "Fan Present", ID_DEBUG_EXHAUST_FAN, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuDebugExhaustFan(&minfoDebugExhaustFan, true, &menuFanTypeExhaust, INFO_LOCATION_RAM);

const SubMenuInfo minfoFanExhaust = { "Exhaust Fan Settings", ID_FAN_EXHAUST_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackFanExhaust(&minfoFanExhaust, &menuDebugExhaustFan, INFO_LOCATION_RAM);
SubMenuItem menuFanExhaust(&minfoFanExhaust, &menuBackFanExhaust, nullptr, INFO_LOCATION_RAM);

// ---- Recirculation Fan submenu ----

const AnalogMenuInfo minfoDebugManualRecirc = {
    "Manual Speed", ID_DEBUG_MANUAL_RECIRC, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuDebugManualRecirc(&minfoDebugManualRecirc, 0, nullptr, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoInvertPwmRecirc = {
    "Invert PWM", ID_INVERT_PWM_RECIRC, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuInvertPwmRecirc(&minfoInvertPwmRecirc, false, &menuDebugManualRecirc, INFO_LOCATION_RAM);

const EnumMenuInfo minfoFanTypeRecirc = {
    "Fan Type", ID_FAN_TYPE_RECIRC, 0xffff, 2, onSettingChanged, fanTypeStrings
};
EnumMenuItem menuFanTypeRecirc(&minfoFanTypeRecirc, 2, &menuInvertPwmRecirc, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoDebugRecircFan = {
    "Fan Present", ID_DEBUG_RECIRC_FAN, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuDebugRecircFan(&minfoDebugRecircFan, true, &menuFanTypeRecirc, INFO_LOCATION_RAM);

const SubMenuInfo minfoFanRecirc = { "Recirc Fan Settings", ID_FAN_RECIRC_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackFanRecirc(&minfoFanRecirc, &menuDebugRecircFan, INFO_LOCATION_RAM);
SubMenuItem menuFanRecirc(&minfoFanRecirc, &menuBackFanRecirc, nullptr, INFO_LOCATION_RAM);

// ---- Network submenu items (bottom-up: last item first) ----

// WiFi IP (read-only, inside Network submenu)
const AnyMenuInfo minfoWifiIp = {
    "WiFi IP", ID_WIFI_IP, 0xffff, 23, NO_CALLBACK
};
TextMenuItem menuWifiIp(&minfoWifiIp, "Not Configured", 24, nullptr, INFO_LOCATION_RAM);

// WiFi Password text item — shows "Is set" / "Not set"
const AnyMenuInfo minfoNetPassword = {
    "Password", ID_NET_PASSWORD, 0xffff, 8, onNetworkEdit
};
TextMenuItem menuNetPassword(&minfoNetPassword, "Not set", 8, &menuWifiIp, INFO_LOCATION_RAM);

// WiFi SSID text item — shows current SSID, click to edit via on-screen keyboard
const AnyMenuInfo minfoNetSsid = {
    "SSID 2.4G", ID_NET_SSID, 0xffff, 33, onNetworkEdit
};
TextMenuItem menuNetSsid(&minfoNetSsid, "", 33, &menuNetPassword, INFO_LOCATION_RAM);

// Network submenu
const SubMenuInfo minfoNetwork = { "Network", ID_NET_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackNetwork(&minfoNetwork, &menuNetSsid, INFO_LOCATION_RAM);
SubMenuItem menuNetwork(&minfoNetwork, &menuBackNetwork, nullptr, INFO_LOCATION_RAM);

// ---- Remaining debug-level items ----

// Chamber Light Present YES/NO
const BooleanMenuInfo minfoDebugChamberLight = {
    "Chamber Light Present", ID_DEBUG_CHAMBER_LIGHT, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuDebugChamberLight(&minfoDebugChamberLight, true, nullptr, INFO_LOCATION_RAM);

const AnalogMenuInfo minfoDebugLogInterval = {
    "Logging Interval", ID_DEBUG_LOG_INTERVAL, 0xffff, 60, onSettingChanged,
    0, 1, "min"
};
AnalogMenuItem menuDebugLogInterval(&minfoDebugLogInterval, 5,
                                    &menuDebugChamberLight, INFO_LOCATION_RAM);

// Debug PID Ki
const AnalogMenuInfo minfoDebugKi = {
    "PID Ki", ID_DEBUG_KI, 0xffff, PID_MAX, onSettingChanged, 0, PID_DIV, ""
};
AnalogMenuItem menuDebugKi(&minfoDebugKi, 50, nullptr, INFO_LOCATION_RAM);

// Debug PID Kp
const AnalogMenuInfo minfoDebugKp = {
    "PID Kp", ID_DEBUG_KP, 0xffff, PID_MAX, onSettingChanged, 0, PID_DIV, ""
};
AnalogMenuItem menuDebugKp(&minfoDebugKp, 200, nullptr, INFO_LOCATION_RAM);

// Debug Bed Temp  [0–100 °C]
const AnalogMenuInfo minfoDebugBed = {
    "Manual Bed Temp", ID_DEBUG_BED, 0xffff, 100, onSettingChanged, 0, 1, "C"
};
AnalogMenuItem menuDebugBed(&minfoDebugBed, 25, nullptr, INFO_LOCATION_RAM);

// Debug Chamber Temp  [0–100 °C]
const AnalogMenuInfo minfoDebugChamber = {
    "Manual Chamber Temp", ID_DEBUG_CHAMBER, 0xffff, 100, onSettingChanged, 0, 1, "C"
};
AnalogMenuItem menuDebugChamber(&minfoDebugChamber, 25, &menuDebugBed, INFO_LOCATION_RAM);

// Debug Mode ON/OFF
const BooleanMenuInfo minfoDebugMode = {
    "Manual Sensor Control", ID_DEBUG_MODE, 0xffff, 1, onSettingChanged, NAMING_ON_OFF
};
BooleanMenuItem menuDebugMode(&minfoDebugMode, false, &menuDebugChamber, INFO_LOCATION_RAM);

// Manual Debug Sensor Control submenu  (contains Debug Mode, Chamber Temp, Bed Temp)
const SubMenuInfo minfoDebugSensor = { "Manual Debug Sens Ctrl", ID_DEBUG_SENSOR_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackDebugSensor(&minfoDebugSensor, &menuDebugMode, INFO_LOCATION_RAM);
SubMenuItem menuDebugSensor(&minfoDebugSensor, &menuBackDebugSensor, nullptr, INFO_LOCATION_RAM);

// 2PIN/3PIN PWM Frequency [10–1000 Hz, default 100]
const AnalogMenuInfo minfoLowPinPwmFreq = {
    "2P/3P PWM Frequency", ID_LOW_PIN_PWM_FREQ, 0xffff, 1000, onSettingChanged, 0, 1, "Hz"
};
AnalogMenuItem menuLowPinPwmFreq(&minfoLowPinPwmFreq, 100, nullptr, INFO_LOCATION_RAM);

// ===========================================================================
// COLD CHAMBER submenu
// ===========================================================================

// Max Chamber Temp  [0–100 °C]
const AnalogMenuInfo minfoColdTemp = {
    "Max Chamber Temp", ID_COLD_TEMP, 0xffff, 100, onSettingChanged, 0, 1, "C"
};
AnalogMenuItem menuColdTemp(&minfoColdTemp, 38, nullptr, INFO_LOCATION_RAM);

// Recirc Speed %
const AnalogMenuInfo minfoColdRecirc = {
    "Recirculation Fan", ID_COLD_RECIRC, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuColdRecirc(&minfoColdRecirc, 20, &menuColdTemp, INFO_LOCATION_RAM);

// Exhaust Min %
const AnalogMenuInfo minfoColdMin = {
    "Exhaust Fan Min", ID_COLD_MIN, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuColdMin(&minfoColdMin, 15, &menuColdRecirc, INFO_LOCATION_RAM);

// Exhaust Max %
const AnalogMenuInfo minfoColdMax = {
    "Exhaust Fan Max", ID_COLD_MAX, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuColdMax(&minfoColdMax, 100, &menuColdMin, INFO_LOCATION_RAM);

const SubMenuInfo minfoCold = { "Cold Chamber Settings", ID_COLD, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackCold(&minfoCold, &menuColdMax, INFO_LOCATION_RAM);

// Cold submenu root (next=nullptr, wired via setNext in menuSetup)
SubMenuItem menuCold(&minfoCold, &menuBackCold, nullptr, INFO_LOCATION_RAM);

// ===========================================================================
// HOT CHAMBER submenu
// ===========================================================================

// Bed threshold  [0–100 °C]
const AnalogMenuInfo minfoHotThreshold = {
    "Bed Threshold", ID_HOT_THRESHOLD, 0xffff, 100, onSettingChanged, 0, 1, "C"
};
AnalogMenuItem menuHotThreshold(&minfoHotThreshold, 60, nullptr, INFO_LOCATION_RAM);

// Exhaust Speed %
const AnalogMenuInfo minfoHotExhaust = {
    "Exhaust Fan Speed", ID_HOT_EXHAUST, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuHotExhaust(&minfoHotExhaust, 15, &menuHotThreshold, INFO_LOCATION_RAM);

// Recirc Speed %
const AnalogMenuInfo minfoHotRecirc = {
    "Recirculation Fan", ID_HOT_RECIRC, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuHotRecirc(&minfoHotRecirc, 50, &menuHotExhaust, INFO_LOCATION_RAM);

// Heating Fan Speed %
const AnalogMenuInfo minfoHotHeating = {
    "Heating Fan Speed", ID_HOT_HEATING, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuHotHeating(&minfoHotHeating, 50, &menuHotRecirc, INFO_LOCATION_RAM);

const SubMenuInfo minfoHot = { "Hot Chamber Settings", ID_HOT, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackHot(&minfoHot, &menuHotHeating, INFO_LOCATION_RAM);

// Hot submenu root (next=&menuCold)
SubMenuItem menuHot(&minfoHot, &menuBackHot, &menuCold, INFO_LOCATION_RAM);

// ===========================================================================
// SETTINGS submenu (top-level)
// ===========================================================================

// Recirc Start Speed %  (next=&menuHot, which puts Hot/Cold/Debug after it)
const AnalogMenuInfo minfoRecircSpeed = {
    "Start Recirc Fan Speed", ID_RECIRC_SPEED, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuRecircSpeed(&minfoRecircSpeed, 30, &menuHot, INFO_LOCATION_RAM);

// Recirc Start Bed Temp  [0–100 °C]
const AnalogMenuInfo minfoRecircTemp = {
    "Startup Bed Temp", ID_RECIRC_TEMP, 0xffff, 100, onSettingChanged, 0, 1, "C"
};
AnalogMenuItem menuRecircTemp(&minfoRecircTemp, 45, &menuRecircSpeed, INFO_LOCATION_RAM);

// Mode Decision Time  [1–60 min]
const AnalogMenuInfo minfoDecisionTime = {
    "Mode Decision Time", ID_DECISION_TIME, 0xffff, 60, onSettingChanged, 0, 1, "min"
};
AnalogMenuItem menuDecisionTime(&minfoDecisionTime, 10, &menuRecircTemp, INFO_LOCATION_RAM);

// Operating Mode enum  0=AUTO  1=HEAT  2=COOL  3=MANUAL
const char opModeStr0[] = "AUTO";
const char opModeStr1[] = "HEATING";
const char opModeStr2[] = "COOLING";
const char opModeStr3m[] = "MANUAL";
const char* const opModeStrings[] = { opModeStr0, opModeStr1, opModeStr2, opModeStr3m };
const EnumMenuInfo minfoOpMode = {
    "Operating Mode", ID_OP_MODE, 0xffff, 3, onSettingChanged, opModeStrings
};
EnumMenuItem menuOpMode(&minfoOpMode, 0, &menuDecisionTime, INFO_LOCATION_RAM);

const SubMenuInfo minfoSettings = { "Settings", ID_SETTINGS, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackSettings(&minfoSettings, &menuOpMode, INFO_LOCATION_RAM);

// Settings submenu root  (next=nullptr — it is the last item in Root)
SubMenuItem menuSettings(&minfoSettings, &menuBackSettings, nullptr, INFO_LOCATION_RAM);

// ===========================================================================
// ROOT — read-only status items
// ===========================================================================

// ===========================================================================
// Fan RPM submenu (read-only tach readings, text items for 2PIN/NA support)
// ===========================================================================

const AnyMenuInfo minfoRpmExhaust = { "Exhaust Fan", ID_RPM_EXHAUST, 0xffff, 12, NO_CALLBACK };
TextMenuItem menuRpmExhaust(&minfoRpmExhaust, "0 RPM", 12, nullptr, INFO_LOCATION_RAM);

const AnyMenuInfo minfoRpmRecirc = { "Recirculation Fan", ID_RPM_RECIRC, 0xffff, 12, NO_CALLBACK };
TextMenuItem menuRpmRecirc(&minfoRpmRecirc, "0 RPM", 12, &menuRpmExhaust, INFO_LOCATION_RAM);

const AnyMenuInfo minfoRpmHeating = { "Heating Fan", ID_RPM_HEATING, 0xffff, 12, NO_CALLBACK };
TextMenuItem menuRpmHeating(&minfoRpmHeating, "0 RPM", 12, &menuRpmRecirc, INFO_LOCATION_RAM);

const SubMenuInfo minfoRpm = { "Fan RPM", ID_RPM_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackRpm(&minfoRpm, &menuRpmHeating, INFO_LOCATION_RAM);
SubMenuItem menuRpm(&minfoRpm, &menuBackRpm, &menuSettings, INFO_LOCATION_RAM);

// Chamber Light on/off — user-editable toggle at root level
const BooleanMenuInfo minfoChamberLight = {
    "Chamber Light", ID_CHAMBER_LIGHT, 0xffff, 1, onSettingChanged, NAMING_ON_OFF
};
BooleanMenuItem menuChamberLight(&minfoChamberLight, false, &menuRpm, INFO_LOCATION_RAM);

// Exhaust Fan %  [RO]
const AnalogMenuInfo minfoExhaustFan = {
    "Exhaust Fan Speed", ID_EXHAUST_FAN, 0xffff, 100, NO_CALLBACK, 0, 1, "%"
};
AnalogMenuItem menuExhaustFan(&minfoExhaustFan, 0, &menuChamberLight, INFO_LOCATION_RAM);

// Recirc Fan %  [RO]
const AnalogMenuInfo minfoRecircFan = {
    "Recirculation Fan Spd", ID_RECIRC_FAN, 0xffff, 100, NO_CALLBACK, 0, 1, "%"
};
AnalogMenuItem menuRecircFan(&minfoRecircFan, 0, &menuExhaustFan, INFO_LOCATION_RAM);

// Heating Fan %  [RO]
const AnalogMenuInfo minfoHeatingFan = {
    "Heating Fan Speed", ID_HEATING_FAN, 0xffff, 100, NO_CALLBACK, 0, 1, "%"
};
AnalogMenuItem menuHeatingFan(&minfoHeatingFan, 0, &menuRecircFan, INFO_LOCATION_RAM);

// Chamber Temp °C  [RO]
const AnalogMenuInfo minfoChamberTemp = {
    "Chamber Temp", ID_CHAMBER_TEMP, 0xffff, 100, NO_CALLBACK, 0, 1, "C"
};
AnalogMenuItem menuChamberTemp(&minfoChamberTemp, 0, &menuHeatingFan, INFO_LOCATION_RAM);

// Bed Temp °C  [RO]
const AnalogMenuInfo minfoBedTemp = {
    "Under-bed Temp", ID_BED_TEMP, 0xffff, 100, NO_CALLBACK, 0, 1, "C"
};
AnalogMenuItem menuBedTemp(&minfoBedTemp, 0, &menuChamberTemp, INFO_LOCATION_RAM);

// Mode enum  [RO]  0=IDLE  1=RECIRC  2=HEAT  3=COOL  4=MANUAL
const char modeStr0[] = "IDLE";
const char modeStr1[] = "RECIRC";
const char modeStr2[] = "HEATING";
const char modeStr3[] = "COOLING";
const char modeStr4[] = "MANUAL";
const char* modeStrings[] = { modeStr0, modeStr1, modeStr2, modeStr3, modeStr4 };
const EnumMenuInfo minfoMode = {
    "Operating Mode", ID_MODE, 0xffff, 4, NO_CALLBACK, modeStrings
};
EnumMenuItem menuMode(&minfoMode, 0, &menuBedTemp, INFO_LOCATION_RAM);

// Root submenu
const SubMenuInfo minfoRoot = { "FanController", 0, 0xffff, 0, NO_CALLBACK };
SubMenuItem menuRoot(&minfoRoot, &menuMode, nullptr, INFO_LOCATION_RAM);

// ===========================================================================
// Dark theme  (white text on black, blue highlight)
// ===========================================================================
static void installDarkTheme() {
    // TcMenu item palette order is: foreground, background, selected background, selected foreground.
    const color_t itemPalette[] = {
        RGB(255, 255, 255), RGB(0, 0, 0), RGB(0, 80, 180), RGB(255, 255, 255)
    };
    TcThemeBuilder themeBuilder(renderer);
    themeBuilder.dimensionsFromRenderer()
        .withSelectedColors(RGB(0, 80, 180), RGB(255, 255, 255))
        .withItemPadding(MenuPadding(2))
        .withRenderingSettings(BaseGraphicalRenderer::NO_TITLE, false)
        .withPalette(itemPalette)
        .withSpacing(1);

    // Native bitmap font: size 2 for 320×240, size 1 for 128×160
#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
    constexpr int menuFontSize = 2;
#else
    constexpr int menuFontSize = 1;
#endif
    themeBuilder.defaultItemProperties()
        .withNativeFont(nullptr, menuFontSize)
        .withPadding(MenuPadding(1))
        .withSpacing(0)
        .withJustification(GridPosition::JUSTIFY_TITLE_LEFT_VALUE_RIGHT)
        .apply();

    themeBuilder.apply();
}

// ===========================================================================
// menuSetup() — called from main setup()
// ===========================================================================
void menuSetup() {
    // Initialise hardware SPI and display
    SPI.begin(PIN_TFT_SCL, -1, PIN_TFT_SDA);  // SCLK, MISO(none), MOSI

#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7735
    gfx.initR(INITR_BLACKTAB);
#elif TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
    gfx.init(TFT_INIT_WIDTH, TFT_INIT_HEIGHT);
    gfx.invertDisplay(false);
#endif

    gfx.setRotation(TFT_ROTATION);
    gfx.fillScreen(0x0000);  // black

    // Turn on backlight
    pinMode(PIN_TFT_BLK, OUTPUT);
    digitalWrite(PIN_TFT_BLK, HIGH);

    // Give ScreenKeyboard a reference to the display (breaks circular dep)
    ScreenKeyboard::setDisplay(gfx);

    // Configure renderer update rate
    renderer.setUpdatesPerSecond(10);

    // Initialise chamber light GPIO — output, default off
    pinMode(PIN_CHAMBER_LIGHT, OUTPUT);
    digitalWrite(PIN_CHAMBER_LIGHT, LOW);

    // Mark RO items so TcMenu won't let the user edit them
    menuMode.setReadOnly(true);
    menuBedTemp.setReadOnly(true);
    menuChamberTemp.setReadOnly(true);
    menuHeatingFan.setReadOnly(true);
    menuRecircFan.setReadOnly(true);
    menuExhaustFan.setReadOnly(true);
    menuWifiIp.setReadOnly(true);
    menuRpmHeating.setReadOnly(true);
    menuRpmRecirc.setReadOnly(true);
    menuRpmExhaust.setReadOnly(true);

    // Initialise switches (must be done before initForEncoder)
    switches.init(internalDigitalIo(), SWITCHES_NO_POLLING, true);

    // Initialise TcMenu with rotary encoder
    // Swap A/B to reverse encoder direction
    menuMgr.initForEncoder(&renderer, &menuMode,
                           PIN_ENC_B, PIN_ENC_A, PIN_ENC_BTN);
    menuMgr.setBackButton(PIN_BTN_K0);

    // Finalize submenu ordering after all items are constructed.
    // PID Kd → Ki → Kp chain inside Exhaust Fan submenu
    menuDebugKd.setNext(&menuDebugKi);
    menuDebugKi.setNext(&menuDebugKp);
    // Wire Settings-level chain: Cold → Fan submenus → Sensor → Network → PWM → Log → Light
    menuCold.setNext(&menuFanHeating);
    menuFanHeating.setNext(&menuFanExhaust);
    menuFanExhaust.setNext(&menuFanRecirc);
    menuFanRecirc.setNext(&menuDebugSensor);
    menuDebugSensor.setNext(&menuNetwork);
    menuNetwork.setNext(&menuLowPinPwmFreq);
    menuLowPinPwmFreq.setNext(&menuDebugLogInterval);
    menuMgr.addChangeNotification(&rootSelectionGuard);

    // Apply dark theme
    installDarkTheme();

    // Sync persisted settings into menu item values
    _loadSettingsToMenu();
    applyFanPresenceToMenu();

    // Apply persisted light state
    digitalWrite(PIN_CHAMBER_LIGHT, gSettings.chamberLightOn ? HIGH : LOW);

    // Show the root menu with Settings selected by default.
    menuMgr.navigateToMenu(&menuMode, &menuSettings, true);

    Serial.println("[Menu] Setup complete");
}

// ===========================================================================
// _loadSettingsToMenu — sync gSettings -> menu items on boot
// ===========================================================================
void _loadSettingsToMenu() {
    menuOpMode.setCurrentValue(static_cast<uint8_t>(gSettings.operatingMode), true);
    menuDecisionTime.setCurrentValue(gSettings.modeDecisionTimeMin, true);
    menuRecircTemp.setCurrentValue(gSettings.recircStartBedTemp, true);
    menuRecircSpeed.setCurrentValue(gSettings.recircStartSpeed, true);

    menuHotHeating.setCurrentValue(gSettings.hot.heatingFanSpeed, true);
    menuHotRecirc.setCurrentValue(gSettings.hot.recircFanSpeed, true);
    menuHotExhaust.setCurrentValue(gSettings.hot.exhaustFanSpeed, true);
    menuHotThreshold.setCurrentValue(gSettings.hot.bedTempThreshold, true);

    menuColdMax.setCurrentValue(gSettings.cold.exhaustFanMax, true);
    menuColdMin.setCurrentValue(gSettings.cold.exhaustFanMin, true);
    menuColdRecirc.setCurrentValue(gSettings.cold.recircFanSpeed, true);
    menuColdTemp.setCurrentValue(gSettings.cold.maxChamberTemp, true);

    menuDebugMode.setBoolean(gSettings.debug.debugMode, true);
    menuDebugChamber.setCurrentValue(static_cast<int>(gSettings.debug.debugChamberTemp), true);
    menuDebugBed.setCurrentValue(static_cast<int>(gSettings.debug.debugBedTemp), true);
    menuDebugManualHeating.setCurrentValue(gSettings.debug.manualHeatingFanSpeed, true);
    menuDebugManualExhaust.setCurrentValue(gSettings.debug.manualExhaustFanSpeed, true);
    menuDebugManualRecirc.setCurrentValue(gSettings.debug.manualRecircFanSpeed, true);
    menuDebugKp.setCurrentValue(static_cast<int>(gSettings.cold.pidKp * PID_DIV), true);
    menuDebugKi.setCurrentValue(static_cast<int>(gSettings.cold.pidKi * PID_DIV), true);
    menuDebugKd.setCurrentValue(static_cast<int>(gSettings.cold.pidKd * PID_DIV), true);
    menuDebugHeatingFan.setBoolean(gSettings.debug.heatingFanPresent, true);
    menuDebugExhaustFan.setBoolean(gSettings.debug.exhaustFanPresent, true);
    menuDebugRecircFan.setBoolean(gSettings.debug.recircFanPresent, true);
    menuDebugChamberLight.setBoolean(gSettings.debug.chamberLightPresent, true);
    menuDebugLogInterval.setCurrentValue(gSettings.debug.logIntervalMin, true);
    menuFanTypeHeating.setCurrentValue(static_cast<uint8_t>(gSettings.debug.heatingFanType), true);
    menuFanTypeExhaust.setCurrentValue(static_cast<uint8_t>(gSettings.debug.exhaustFanType), true);
    menuFanTypeRecirc.setCurrentValue(static_cast<uint8_t>(gSettings.debug.recircFanType), true);
    menuInvertPwmHeating.setBoolean(gSettings.debug.heatingFanInvertPwm, true);
    menuInvertPwmExhaust.setBoolean(gSettings.debug.exhaustFanInvertPwm, true);
    menuInvertPwmRecirc.setBoolean(gSettings.debug.recircFanInvertPwm, true);
    menuLowPinPwmFreq.setCurrentValue(gSettings.lowPinPwmFreqHz, true);
    menuChamberLight.setBoolean(gSettings.chamberLightOn, true);

    // Show current SSID in the text item
    menuNetSsid.setTextValue(gSettings.network.ssid, true);
    menuNetPassword.setTextValue(
        gSettings.network.password[0] ? "Is set" : "Not set", true);
}

void menuSyncSettings() {
    _loadSettingsToMenu();
    applyFanPresenceToMenu();
}

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
    // Always restore display text (TcMenu's editor may have overwritten it)
    menuNetSsid.setTextValue(gSettings.network.ssid, true);
    menuNetPassword.setTextValue(
        gSettings.network.password[0] ? "Is set" : "Not set", true);
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

static const char* _modeShortLabel(uint8_t stateModeIndex) {
    switch (stateModeIndex) {
        case 0: return "IDLE";
        case 1: return "RECIRC";
        case 2: return gSettings.debug.heatingFanPresent ? "HEATING" : "HOT";
        case 3: return gSettings.debug.exhaustFanPresent ? "COOLING" : "COOL";
        case 4: return "MANUAL";
        default: return "UNKN";
    }
}

void applyFanPresenceToMenu() {
    const bool hFan = gSettings.debug.heatingFanPresent;
    const bool cFan = gSettings.debug.exhaustFanPresent;
    const bool rFan = gSettings.debug.recircFanPresent;

    // Heating fan items
    menuHeatingFan.setVisible(hFan);    // root status item
    menuHotHeating.setVisible(hFan);

    // Exhaust fan items
    menuHotExhaust.setVisible(cFan);
    menuColdMax.setVisible(cFan);
    menuColdMin.setVisible(cFan);
    menuColdTemp.setVisible(cFan);

    // Recirc fan items
    menuRecircFan.setVisible(rFan);     // root status item
    menuRecircTemp.setVisible(rFan);
    menuRecircSpeed.setVisible(rFan);
    menuHotRecirc.setVisible(rFan);
    menuColdRecirc.setVisible(rFan);

    // RPM submenu items
    menuRpmHeating.setVisible(hFan);
    menuRpmRecirc.setVisible(rFan);
    menuRpmExhaust.setVisible(cFan);

    // Sync root mode enum labels to fan presence
    modeStrings[2] = hFan ? "HEATING" : "HOT";
    modeStrings[3] = cFan ? "COOLING" : "COOL";
    menuMode.setChanged(true);

    // Chamber light toggle (hidden when light hardware not installed)
    menuChamberLight.setVisible(gSettings.debug.chamberLightPresent);
}

static uint16_t _modeFooterBackground(uint8_t stateModeIndex) {
    switch (stateModeIndex) {
        case 0: return RGB(32, 32, 32);    // IDLE: neutral dark gray
        case 1: return RGB(0, 128, 128);   // RECI: teal
        case 2: return RGB(200, 0, 0);     // HEAT: strong red
        case 3: return RGB(0, 0, 255);     // COOL: blue
        case 4: return RGB(180, 100, 0);   // MANUAL: orange
        default: return RGB(0, 0, 0);
    }
}

static void _drawFooterStatus(uint8_t stateModeIndex, float chamberTempC) {
    constexpr uint16_t FOOTER_FG = RGB(255, 255, 255);
    const int16_t footerY = TFT_MENU_HEIGHT;
    const int16_t footerH = TFT_RESERVED_BOTTOM_PX;
#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
    const int16_t textX = 12;
    const int16_t textBaselineY = footerY + 52;
#else
    const int16_t textX = 4;
    const int16_t textBaselineY = footerY + 7;
#endif
    const uint16_t footerBg = _modeFooterBackground(stateModeIndex);
    const int displayTempC =
        (chamberTempC <= TEMP_READ_ERROR) ? 0 : static_cast<int>(roundf(chamberTempC));

    char statusBuffer[24];
    snprintf(statusBuffer, sizeof(statusBuffer), "%s %d C",
             _modeShortLabel(stateModeIndex),
             displayTempC);

    gfx.fillRect(0, footerY, TFT_WIDTH, footerH, footerBg);

    gfx.setTextWrap(false);
    gfx.setTextColor(FOOTER_FG);
#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setTextSize(2);
#else
    gfx.setFont(nullptr);          // bitmap font, bold blocky look
    gfx.setTextSize(2);            // 12×16 px per char — fills 30px footer well
#endif
    gfx.setCursor(textX, textBaselineY);
    gfx.print(statusBuffer);
    gfx.setFont(nullptr);
}

static bool _isTopLevelStatusItem(MenuItem* item) {
    return item == &menuMode ||
           item == &menuBedTemp ||
           item == &menuChamberTemp ||
           item == &menuHeatingFan ||
           item == &menuRecircFan ||
           item == &menuExhaustFan;
}

void menuSetWifiIpStatus(const char* wifiStatusText) {
    menuWifiIp.setTextValue(wifiStatusText ? wifiStatusText : "Not Connected", true);
}

// ===========================================================================
// menuUpdateStatus — refresh RO display items from system state
// ===========================================================================
void menuUpdateStatus(uint8_t  stateModeIndex,
                      float    bedTempC,
                      float    chamberTempC,
                      uint8_t  heatingFanPct,
                      uint8_t  recircFanPct,
                      uint8_t  exhaustFanPct) {
    menuMode.setCurrentValue(stateModeIndex);
    menuBedTemp.setCurrentValue(constrain(static_cast<int>(bedTempC), 0, 100));
    menuChamberTemp.setCurrentValue(constrain(static_cast<int>(chamberTempC), 0, 100));
    menuHeatingFan.setCurrentValue(heatingFanPct);
    menuRecircFan.setCurrentValue(recircFanPct);
    menuExhaustFan.setCurrentValue(exhaustFanPct);
}

void menuUpdateRpm(uint16_t heatingRpm, uint16_t recircRpm, uint16_t exhaustRpm) {
    char buf[12];
    if (gSettings.debug.heatingFanType == FanType::Pin2) {
        menuRpmHeating.setTextValue("2PIN/NA", true);
    } else {
        snprintf(buf, sizeof(buf), "%u RPM", heatingRpm);
        menuRpmHeating.setTextValue(buf, true);
    }
    if (gSettings.debug.recircFanType == FanType::Pin2) {
        menuRpmRecirc.setTextValue("2PIN/NA", true);
    } else {
        snprintf(buf, sizeof(buf), "%u RPM", recircRpm);
        menuRpmRecirc.setTextValue(buf, true);
    }
    if (gSettings.debug.exhaustFanType == FanType::Pin2) {
        menuRpmExhaust.setTextValue("2PIN/NA", true);
    } else {
        snprintf(buf, sizeof(buf), "%u RPM", exhaustRpm);
        menuRpmExhaust.setTextValue(buf, true);
    }
}

void menuUpdateFooterStatus(uint8_t stateModeIndex, float chamberTempC) {
    _drawFooterStatus(stateModeIndex, chamberTempC);
}

// ===========================================================================
// onSettingChanged — copies changed value from menu into gSettings and saves
// ===========================================================================
void onSettingChanged(int id) {
    switch (id) {
        case ID_OP_MODE:
            gSettings.operatingMode =
                static_cast<OperatingMode>(menuOpMode.getCurrentValue());
            break;
        case ID_DECISION_TIME:
            gSettings.modeDecisionTimeMin = menuDecisionTime.getCurrentValue();
            break;
        case ID_RECIRC_TEMP:
            gSettings.recircStartBedTemp = menuRecircTemp.getCurrentValue();
            break;
        case ID_RECIRC_SPEED:
            gSettings.recircStartSpeed = menuRecircSpeed.getCurrentValue();
            break;
        case ID_HOT_HEATING:
            gSettings.hot.heatingFanSpeed = menuHotHeating.getCurrentValue();
            break;
        case ID_HOT_RECIRC:
            gSettings.hot.recircFanSpeed = menuHotRecirc.getCurrentValue();
            break;
        case ID_HOT_EXHAUST:
            gSettings.hot.exhaustFanSpeed = menuHotExhaust.getCurrentValue();
            break;
        case ID_HOT_THRESHOLD:
            gSettings.hot.bedTempThreshold = menuHotThreshold.getCurrentValue();
            break;
        case ID_COLD_MAX:
            gSettings.cold.exhaustFanMax = menuColdMax.getCurrentValue();
            break;
        case ID_COLD_MIN:
            gSettings.cold.exhaustFanMin = menuColdMin.getCurrentValue();
            break;
        case ID_COLD_RECIRC:
            gSettings.cold.recircFanSpeed = menuColdRecirc.getCurrentValue();
            break;
        case ID_COLD_TEMP:
            gSettings.cold.maxChamberTemp = menuColdTemp.getCurrentValue();
            break;
        case ID_DEBUG_MODE:
            gSettings.debug.debugMode = menuDebugMode.getBoolean();
            Serial.printf("[Menu] Debug mode: %s\n",
                          gSettings.debug.debugMode ? "ON" : "OFF");
            break;
        case ID_DEBUG_CHAMBER:
            gSettings.debug.debugChamberTemp =
                static_cast<float>(menuDebugChamber.getCurrentValue());
            break;
        case ID_DEBUG_BED:
            gSettings.debug.debugBedTemp =
                static_cast<float>(menuDebugBed.getCurrentValue());
            break;
        case ID_DEBUG_MANUAL_HEATING:
            gSettings.debug.manualHeatingFanSpeed = menuDebugManualHeating.getCurrentValue();
            break;
        case ID_DEBUG_MANUAL_EXHAUST:
            gSettings.debug.manualExhaustFanSpeed = menuDebugManualExhaust.getCurrentValue();
            break;
        case ID_DEBUG_MANUAL_RECIRC:
            gSettings.debug.manualRecircFanSpeed = menuDebugManualRecirc.getCurrentValue();
            break;
        case ID_DEBUG_KP:
            gSettings.cold.pidKp = menuDebugKp.getCurrentValue() / float(PID_DIV);
            break;
        case ID_DEBUG_KI:
            gSettings.cold.pidKi = menuDebugKi.getCurrentValue() / float(PID_DIV);
            break;
        case ID_DEBUG_KD:
            gSettings.cold.pidKd = menuDebugKd.getCurrentValue() / float(PID_DIV);
            break;
        case ID_DEBUG_HEATING_FAN:
            gSettings.debug.heatingFanPresent = menuDebugHeatingFan.getBoolean();
            applyFanPresenceToMenu();
            applyFanPresence();
            break;
        case ID_DEBUG_EXHAUST_FAN:
            gSettings.debug.exhaustFanPresent = menuDebugExhaustFan.getBoolean();
            applyFanPresenceToMenu();
            applyFanPresence();
            break;
        case ID_DEBUG_RECIRC_FAN:
            gSettings.debug.recircFanPresent = menuDebugRecircFan.getBoolean();
            applyFanPresenceToMenu();
            applyFanPresence();
            break;
        case ID_DEBUG_CHAMBER_LIGHT:
            gSettings.debug.chamberLightPresent = menuDebugChamberLight.getBoolean();
            applyFanPresenceToMenu();
            break;
        case ID_DEBUG_LOG_INTERVAL:
            gSettings.debug.logIntervalMin =
                static_cast<uint8_t>(menuDebugLogInterval.getCurrentValue());
            break;
        case ID_FAN_TYPE_HEATING:
            gSettings.debug.heatingFanType =
                static_cast<FanType>(menuFanTypeHeating.getCurrentValue());
            applyFanFrequencies();
            break;
        case ID_FAN_TYPE_EXHAUST:
            gSettings.debug.exhaustFanType =
                static_cast<FanType>(menuFanTypeExhaust.getCurrentValue());
            applyFanFrequencies();
            break;
        case ID_FAN_TYPE_RECIRC:
            gSettings.debug.recircFanType =
                static_cast<FanType>(menuFanTypeRecirc.getCurrentValue());
            applyFanFrequencies();
            break;
        case ID_INVERT_PWM_HEATING:
            gSettings.debug.heatingFanInvertPwm = menuInvertPwmHeating.getBoolean();
            applyFanPresence();  // re-applies invert flag to FanController
            break;
        case ID_INVERT_PWM_EXHAUST:
            gSettings.debug.exhaustFanInvertPwm = menuInvertPwmExhaust.getBoolean();
            applyFanPresence();
            break;
        case ID_INVERT_PWM_RECIRC:
            gSettings.debug.recircFanInvertPwm = menuInvertPwmRecirc.getBoolean();
            applyFanPresence();
            break;
        case ID_LOW_PIN_PWM_FREQ:
            gSettings.lowPinPwmFreqHz =
                static_cast<uint16_t>(menuLowPinPwmFreq.getCurrentValue());
            applyFanFrequencies();
            break;
        case ID_CHAMBER_LIGHT:
            gSettings.chamberLightOn = menuChamberLight.getBoolean();
            digitalWrite(PIN_CHAMBER_LIGHT, gSettings.chamberLightOn ? HIGH : LOW);
            Serial.printf("[Menu] Chamber light: %s\n",
                          gSettings.chamberLightOn ? "ON" : "OFF");
            break;
        default:
            return;  // Unknown id — skip save
    }
    gSettings.save();
    Serial.printf("[Menu] Setting id=%d saved\n", id);
}
