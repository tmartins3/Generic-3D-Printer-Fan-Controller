#include "MenuSetup.h"
#include "../../include/Config.h"
#include "../settings/Settings.h"

#include <SPI.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

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
    bool menuEditStarting(MenuItem* item) override { return true; }
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

// Debug PID Kd  [0.00–10.00, stored as 0–1000]
const AnalogMenuInfo minfoDebugKd = {
    "PID Kd", ID_DEBUG_KD, 0xffff, PID_MAX, onSettingChanged, 0, PID_DIV, ""
};
AnalogMenuItem menuDebugKd(&minfoDebugKd, 100, nullptr, INFO_LOCATION_RAM);

const SubMenuInfo minfoDebugPid = { "Cooling PID", ID_DEBUG_PID_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackDebugPid(&minfoDebugPid, &menuDebugKd, INFO_LOCATION_RAM);
SubMenuItem menuDebugPid(&minfoDebugPid, &menuBackDebugPid, nullptr, INFO_LOCATION_RAM);

const AnalogMenuInfo minfoDebugManualRecirc = {
    "Recirculation Fan", ID_DEBUG_MANUAL_RECIRC, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuDebugManualRecirc(&minfoDebugManualRecirc, 0, nullptr, INFO_LOCATION_RAM);

const AnalogMenuInfo minfoDebugManualExhaust = {
    "Exhaust Fan Speed", ID_DEBUG_MANUAL_EXHAUST, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuDebugManualExhaust(&minfoDebugManualExhaust, 0, &menuDebugManualRecirc, INFO_LOCATION_RAM);

const AnalogMenuInfo minfoDebugManualHeating = {
    "Heating Fan Speed", ID_DEBUG_MANUAL_HEATING, 0xffff, 100, onSettingChanged, 0, 1, "%"
};
AnalogMenuItem menuDebugManualHeating(&minfoDebugManualHeating, 0, &menuDebugManualExhaust, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoDebugManualEnable = {
    "Manual Control", ID_DEBUG_MANUAL_ENABLE, 0xffff, 1, onSettingChanged, NAMING_ON_OFF
};
BooleanMenuItem menuDebugManualEnable(&minfoDebugManualEnable, false, &menuDebugManualHeating, INFO_LOCATION_RAM);

// Fan presence flags  (added after Manual Fan Control in the Debug menu chain)
const BooleanMenuInfo minfoDebugRecircFan = {
    "Recirc Fan Present", ID_DEBUG_RECIRC_FAN, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuDebugRecircFan(&minfoDebugRecircFan, true, nullptr, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoDebugExhaustFan = {
    "Exhaust Fan Present", ID_DEBUG_EXHAUST_FAN, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuDebugExhaustFan(&minfoDebugExhaustFan, true, &menuDebugRecircFan, INFO_LOCATION_RAM);

const BooleanMenuInfo minfoDebugHeatingFan = {
    "Heating Fan Present", ID_DEBUG_HEATING_FAN, 0xffff, 1, onSettingChanged, NAMING_YES_NO
};
BooleanMenuItem menuDebugHeatingFan(&minfoDebugHeatingFan, true, &menuDebugExhaustFan, INFO_LOCATION_RAM);

const SubMenuInfo minfoDebugManual = { "Manual Fan Control", ID_DEBUG_MANUAL_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackDebugManual(&minfoDebugManual, &menuDebugManualEnable, INFO_LOCATION_RAM);
SubMenuItem menuDebugManual(&minfoDebugManual, &menuBackDebugManual, &menuDebugPid, INFO_LOCATION_RAM);

// Debug PID Ki
const AnalogMenuInfo minfoDebugKi = {
    "PID Ki", ID_DEBUG_KI, 0xffff, PID_MAX, onSettingChanged, 0, PID_DIV, ""
};
AnalogMenuItem menuDebugKi(&minfoDebugKi, 50, &menuDebugKd, INFO_LOCATION_RAM);

// Debug PID Kp
const AnalogMenuInfo minfoDebugKp = {
    "PID Kp", ID_DEBUG_KP, 0xffff, PID_MAX, onSettingChanged, 0, PID_DIV, ""
};
AnalogMenuItem menuDebugKp(&minfoDebugKp, 200, &menuDebugKi, INFO_LOCATION_RAM);

// Debug Bed Temp  [0–100 °C]
const AnalogMenuInfo minfoDebugBed = {
    "Debug Bed Temp", ID_DEBUG_BED, 0xffff, 100, onSettingChanged, 0, 1, "C"
};
AnalogMenuItem menuDebugBed(&minfoDebugBed, 25, nullptr, INFO_LOCATION_RAM);

// Debug Chamber Temp  [0–100 °C]
const AnalogMenuInfo minfoDebugChamber = {
    "Debug Chamber Temp", ID_DEBUG_CHAMBER, 0xffff, 100, onSettingChanged, 0, 1, "C"
};
AnalogMenuItem menuDebugChamber(&minfoDebugChamber, 25, &menuDebugBed, INFO_LOCATION_RAM);

// Debug Mode ON/OFF
const BooleanMenuInfo minfoDebugMode = {
    "Debug Mode", ID_DEBUG_MODE, 0xffff, 1, onSettingChanged, NAMING_ON_OFF
};
BooleanMenuItem menuDebugMode(&minfoDebugMode, false, &menuDebugChamber, INFO_LOCATION_RAM);

// Manual Sensor Control submenu  (contains Debug Mode, Chamber Temp, Bed Temp)
const SubMenuInfo minfoDebugSensor = { "Manual Sensor Control", ID_DEBUG_SENSOR_MENU, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackDebugSensor(&minfoDebugSensor, &menuDebugMode, INFO_LOCATION_RAM);
SubMenuItem menuDebugSensor(&minfoDebugSensor, &menuBackDebugSensor, &menuDebugManual, INFO_LOCATION_RAM);

// Back item for Debug submenu
const SubMenuInfo minfoDebug = { "Debug Settings", ID_DEBUG, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackDebug(&minfoDebug, &menuDebugSensor, INFO_LOCATION_RAM);

// Debug submenu root  (next=nullptr because it is the last item in Settings)
SubMenuItem menuDebug(&minfoDebug, &menuBackDebug, nullptr, INFO_LOCATION_RAM);

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

// Cold submenu root (next=&menuDebug, placing Debug after Cold in Settings list)
SubMenuItem menuCold(&minfoCold, &menuBackCold, &menuDebug, INFO_LOCATION_RAM);

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

// Operating Mode enum  0=AUTO  1=HEAT  2=COOL
const char opModeStr0[] = "AUTO";
const char opModeStr1[] = "HEATING";
const char opModeStr2[] = "COOLING";
const char* const opModeStrings[] = { opModeStr0, opModeStr1, opModeStr2 };
const EnumMenuInfo minfoOpMode = {
    "Operating Mode", ID_OP_MODE, 0xffff, 2, onSettingChanged, opModeStrings
};
EnumMenuItem menuOpMode(&minfoOpMode, 0, &menuDecisionTime, INFO_LOCATION_RAM);

const SubMenuInfo minfoSettings = { "Settings", ID_SETTINGS, 0xffff, 0, NO_CALLBACK };
BackMenuItem menuBackSettings(&minfoSettings, &menuOpMode, INFO_LOCATION_RAM);

// Settings submenu root  (next=nullptr — it is the last item in Root)
SubMenuItem menuSettings(&minfoSettings, &menuBackSettings, nullptr, INFO_LOCATION_RAM);

// ===========================================================================
// ROOT — read-only status items
// ===========================================================================

const AnyMenuInfo minfoWifiIp = {
    "WiFi IP", ID_WIFI_IP, 0xffff, 23, NO_CALLBACK
};
TextMenuItem menuWifiIp(&minfoWifiIp, "Not Connected", 24, &menuSettings, INFO_LOCATION_RAM);

// Exhaust Fan %  [RO]  — next=&menuSettings
const AnalogMenuInfo minfoExhaustFan = {
    "Exhaust Fan Speed", ID_EXHAUST_FAN, 0xffff, 100, NO_CALLBACK, 0, 1, "%"
};
AnalogMenuItem menuExhaustFan(&minfoExhaustFan, 0, &menuWifiIp, INFO_LOCATION_RAM);

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

// Mode enum  [RO]  0=IDLE  1=RECIRC  2=HEAT  3=COOL
const char modeStr0[] = "IDLE";
const char modeStr1[] = "RECIRC";
const char modeStr2[] = "HEATING";
const char modeStr3[] = "COOLING";
const char* modeStrings[] = { modeStr0, modeStr1, modeStr2, modeStr3 };
const EnumMenuInfo minfoMode = {
    "Operating Mode", ID_MODE, 0xffff, 3, NO_CALLBACK, modeStrings
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

    themeBuilder.defaultItemProperties()
        .withNativeFont(nullptr, 2)
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

    // Configure renderer update rate
    renderer.setUpdatesPerSecond(10);

    // Mark RO items so TcMenu won't let the user edit them
    menuMode.setReadOnly(true);
    menuBedTemp.setReadOnly(true);
    menuChamberTemp.setReadOnly(true);
    menuHeatingFan.setReadOnly(true);
    menuRecircFan.setReadOnly(true);
    menuExhaustFan.setReadOnly(true);
    menuWifiIp.setReadOnly(true);

    // Initialise switches (must be done before initForEncoder)
    switches.init(internalDigitalIo(), SWITCHES_NO_POLLING, true);

    // Initialise TcMenu with rotary encoder
    // Swap A/B to reverse encoder direction
    menuMgr.initForEncoder(&renderer, &menuMode,
                           PIN_ENC_B, PIN_ENC_A, PIN_ENC_BTN);
    menuMgr.setBackButton(PIN_BTN_K0);

    // Finalize debug submenu ordering after all items are constructed.
    menuBackDebugPid.setNext(&menuDebugKp);
    menuDebugPid.setNext(&menuDebugHeatingFan);
    menuMgr.addChangeNotification(&rootSelectionGuard);

    // Apply dark theme
    installDarkTheme();

    // Sync persisted settings into menu item values
    _loadSettingsToMenu();
    applyFanPresenceToMenu();

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
    menuDebugManualEnable.setBoolean(gSettings.debug.manualFanControl, true);
    menuDebugManualHeating.setCurrentValue(gSettings.debug.manualHeatingFanSpeed, true);
    menuDebugManualExhaust.setCurrentValue(gSettings.debug.manualExhaustFanSpeed, true);
    menuDebugManualRecirc.setCurrentValue(gSettings.debug.manualRecircFanSpeed, true);
    menuDebugKp.setCurrentValue(static_cast<int>(gSettings.cold.pidKp * PID_DIV), true);
    menuDebugKi.setCurrentValue(static_cast<int>(gSettings.cold.pidKi * PID_DIV), true);
    menuDebugKd.setCurrentValue(static_cast<int>(gSettings.cold.pidKd * PID_DIV), true);
    menuDebugHeatingFan.setBoolean(gSettings.debug.heatingFanPresent, true);
    menuDebugExhaustFan.setBoolean(gSettings.debug.exhaustFanPresent, true);
    menuDebugRecircFan.setBoolean(gSettings.debug.recircFanPresent, true);
}

static const char* _modeShortLabel(uint8_t stateModeIndex) {
    switch (stateModeIndex) {
        case 0: return "IDLE";
        case 1: return "RECIRC";
        case 2: return gSettings.debug.heatingFanPresent ? "HEATING" : "HOT";
        case 3: return gSettings.debug.exhaustFanPresent ? "COOLING" : "COOL";
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

    // Sync root mode enum labels to fan presence
    modeStrings[2] = hFan ? "HEATING" : "HOT";
    modeStrings[3] = cFan ? "COOLING" : "COOL";
    menuMode.setChanged(true);
}

static uint16_t _modeFooterBackground(uint8_t stateModeIndex) {
    switch (stateModeIndex) {
        case 0: return RGB(32, 32, 32);    // IDLE: neutral dark gray
        case 1: return RGB(0, 128, 128);   // RECI: teal
        case 2: return RGB(200, 0, 0);     // HEAT: strong red
        case 3: return RGB(0, 0, 255);     // COOL: blue
        default: return RGB(0, 0, 0);
    }
}

static void _drawFooterStatus(uint8_t stateModeIndex, float chamberTempC) {
    constexpr uint16_t FOOTER_FG = RGB(255, 255, 255);
    const int16_t footerY = TFT_MENU_HEIGHT;
    const int16_t footerH = TFT_RESERVED_BOTTOM_PX;
    const int16_t textX = 12;
    const int16_t textBaselineY = footerY + 52;
    const uint16_t footerBg = _modeFooterBackground(stateModeIndex);
    const int displayTempC =
        (chamberTempC <= TEMP_READ_ERROR) ? 0 : static_cast<int>(roundf(chamberTempC));

    char statusBuffer[24];
    snprintf(statusBuffer, sizeof(statusBuffer), "%s %d C",
             _modeShortLabel(stateModeIndex),
             displayTempC);

    gfx.fillRect(0, footerY, TFT_WIDTH, footerH, footerBg);

    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setTextWrap(false);
    gfx.setTextColor(FOOTER_FG);
    gfx.setTextSize(2);
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
           item == &menuExhaustFan ||
           item == &menuWifiIp;
}

void menuSetWifiIpStatus(const char* wifiStatusText) {
    menuWifiIp.setTextValue(wifiStatusText ? wifiStatusText : "Not Connected", true);
}

uint8_t menuGetConfiguredOperatingModeIndex() {
    return static_cast<uint8_t>(menuOpMode.getCurrentValue());
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
        case ID_DEBUG_MANUAL_ENABLE:
            gSettings.debug.manualFanControl = menuDebugManualEnable.getBoolean();
            Serial.printf("[Menu] Manual fan control: %s\n",
                          gSettings.debug.manualFanControl ? "ON" : "OFF");
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
            break;
        case ID_DEBUG_EXHAUST_FAN:
            gSettings.debug.exhaustFanPresent = menuDebugExhaustFan.getBoolean();
            applyFanPresenceToMenu();
            break;
        case ID_DEBUG_RECIRC_FAN:
            gSettings.debug.recircFanPresent = menuDebugRecircFan.getBoolean();
            applyFanPresenceToMenu();
            break;
        default:
            return;  // Unknown id — skip save
    }
    gSettings.save();
    Serial.printf("[Menu] Setting id=%d saved\n", id);
}
