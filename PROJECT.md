# FanController

## Goal

The goal is to make software for a 3D printer enclosure fan controller.

The printer is an enclosed Prusa XL with 24 V power available.

The hardware consists of:

- One 24 V radial fan with a filter for cooling and exhaust filtering
- One 24 V radial fan with a filter for recirculation filtering
- One 12 V fan under the bed to move more heat from under the bed

Each fan can be **4-pin** (PWM + tach), **3-pin** (tach only, speed via
MOSFET), or **2-pin** (no tach, speed via MOSFET). The fan type for each
fan is configurable in the per-fan settings submenu. 2-pin and 3-pin fans
require an external N-channel MOSFET on the power line for speed control
and use a lower configurable PWM frequency (default 100 Hz). 4-pin fans
use the standard 25 kHz PWM signal.

## Temperature Sensors

- Chamber temperature sensor: `DS18B20`
- Under-bed temperature sensor, placed outside the fan airflow, to monitor the temperature the bed was heated to

The under-bed sensor is used to:

- Automatically decide whether the chamber should be actively heated or cooled using the exhaust fan
- Automatically shut off all fans when the bed has cooled and printing is over

## Hardware

- Two step-down converters:
  - One 24 V -> 5 V converter for the ESP32 and screen
  - One 24 V -> 12 V converter for the under-bed fan
- ESP32 board
- TFT + encoder + button module, with one extra button in addition to the button integrated into the encoder
  - Supported drivers: `ST7789` (320×240) and `ST7735` (128×160)
  - Current configured driver in firmware: `ST7735`
  - Current configured logical resolution (post-rotation): `160x128`
  - Current configured controller init size: `128x160`
  - Current configured rotation: `1`
  - Current configured inversion: `invertDisplay(false)`

## UI

- The UI is created using the `TcMenu` library.
- The first menu level functions as the main display screen.
- The main display should show the live system status using read-only menu items.
- Settings are accessed from the same top-level menu structure.
- The bottom of the screen is reserved for a large custom status footer
  (`80 px` on ST7789, `30 px` on ST7735).
- At boot, the root screen opens with `Settings` selected.
- The read-only status rows above `Settings` remain visible but are not intended as selectable navigation targets.
- Errors should first be implemented using a small status indicator such as a title widget or similar TcMenu-supported indicator.
- After implementation, the visual result should be evaluated on the real display and adjusted if needed.

## Pin Mapping

Planned wiring between the TFT + encoder module and the ESP32 (`az-delivery-devkit-v4`):

- TFT `GND` -> ESP32 `GND`
- TFT `VCC` -> ESP32 `5V`
- TFT `SCL` -> ESP32 `GPIO18`
- TFT `SDA` -> ESP32 `GPIO23`
- TFT `RES` -> ESP32 `GPIO4`
- TFT `DC` -> ESP32 `GPIO16`
- TFT `CS` -> ESP32 `GPIO17`
- TFT `BLK` -> ESP32 `GPIO21`
- Encoder `A` -> ESP32 `GPIO32`
- Encoder `B` -> ESP32 `GPIO33`
- Encoder `PUSH` -> ESP32 `GPIO25`
- Extra button `K0` -> ESP32 `GPIO26` (mapped as TcMenu back button)
- Exhaust fan PWM -> ESP32 `GPIO27`
- Exhaust fan tach -> ESP32 `GPIO34`
- Recirculation fan PWM -> ESP32 `GPIO14`
- Recirculation fan tach -> ESP32 `GPIO35`
- Under-bed fan PWM -> ESP32 `GPIO13`
- Under-bed fan tach -> ESP32 `GPIO39`
- Chamber temperature sensor (DS18B20) -> ESP32 `GPIO19` (separate 1-Wire bus)
- Under-bed temperature sensor (DS18B20) -> ESP32 `GPIO22` (separate 1-Wire bus)
- Chamber light (24 V LED via MOSFET) -> ESP32 `GPIO5`

Notes:

- `GPIO18` and `GPIO23` use the ESP32's normal hardware SPI pins for clock and MOSI.
- This display module appears to be write-only SPI, so no TFT `MISO` pin is expected.
- `BLK` can be driven directly from a GPIO for simple backlight on/off control. If brightness control is needed, use PWM on the same pin.
- The TFT module family is commonly sold as 3.3 V or 3.3 V/5 V compatible. Before wiring permanently, confirm whether `VCC` should be connected to `5V` or `3V3` on the exact board revision you have.
- The encoder and buttons should be configured as GPIO inputs, likely using pull-ups if the module does not already provide them.
- `GPIO34`, `GPIO35`, and `GPIO39` are input-only, which makes them suitable for fan tachometer inputs.
- Each fan tach input should have a pull-up because 4-pin PC fan tach outputs are typically open-collector.

## Firmware Behavior

Describe how the firmware should behave.

### Startup

- The controller starts in `IDLE` state on boot.
- All fans are off at startup.
- No splash screen — go directly to the main display.

### Main UI

`RO` = read-only

- Main screen
  - Operating mode: `IDLE` / `RECIRC` / `HEATING` / `COOLING` /
    `MANUAL` (`RO`)
  - Under-bed temperature in C (`RO`)
  - Chamber temperature in C (`RO`)
  - Under-bed fan speed % (`RO`)
  - Recirculation fan speed % (`RO`)
  - Exhaust fan speed % (`RO`)
  - WiFi IP (`RO`) or `Not Connected`
  - Fan RPM submenu (`RO`) — shows tach RPM for each fan;
    2-pin fans display "2PIN/NA" instead of a numeric value
  - Large bottom status footer
    - Mode shorthand plus chamber temperature, e.g. `COOL 27 C`
    - White text with mode-dependent background color:
      - `COOLING`: blue
      - `HEATING`: red
      - `RECIRC`: teal
      - `IDLE`: dark gray
      - `MANUAL`: orange
- Settings
  - Operating mode: `AUTO` / `HEATING` / `COOLING` / `MANUAL`
  - Mode decision time (default: 10 min)
  - Recirculation startup bed temperature in C (default: 45)
  - Recirculation fan start speed % (default: 30%)
  - Hot chamber settings
    - Heater fan speed % (default: 50%)
    - Recirculation fan % (default: 50%)
    - Exhaust fan % (default: 15%)
    - Hot chamber bed temperature threshold (default: 60 C)
  - Cold chamber settings
    - Exhaust fan max % (default: 100%)
    - Exhaust fan min speed % (default: 15%)
    - Recirculation fan speed % (default: 20%)
    - Max cold chamber temperature (default: 38 C)
  - Heating Fan Settings
    - Heating Fan Present ON/OFF
    - Fan Type: 2PIN / 3PIN / 4PIN
    - Manual Speed %
  - Exhaust Fan Settings
    - Exhaust Fan Present ON/OFF
    - Fan Type: 2PIN / 3PIN / 4PIN
    - Manual Speed %
    - PID `Kd` (default: 1.0)
    - PID `Ki` (default: 0.5)
    - PID `Kp` (default: 2.0)
  - Recirc Fan Settings
    - Recirc Fan Present ON/OFF
    - Fan Type: 2PIN / 3PIN / 4PIN
    - Manual Speed %
  - Manual Debug Sens Ctrl
    - Manual Sensor Control ON/OFF
    - Manual Chamber Temp C
    - Manual Bed Temp C
  - Network
    - SSID 2.4G (text item, shows current SSID,
      launches T9 keyboard on click)
    - Password (text item, shows "Is set" / "Not set",
      launches T9 keyboard on click)
    - WiFi IP (`RO`)
  - 2P/3P PWM Frequency (default: 100 Hz, range 0–1000)
  - Logging Interval (default: 5 min)
  - Chamber Light Present ON/OFF

All setting changes must be persisted immediately when changed.

Boot-time runtime overrides:

- `Debug Mode` is forced `OFF` at boot
- `Manual Fan Control` is forced `OFF` at boot
- `MANUAL` operating mode is reset to `AUTO` at boot

### Control Logic

The software has five internal states:

- `IDLE`: all fans off
  - Active when the bed temperature is below `Recirculation fan startup bed temp` (`RFSBT`)
- `RECIRCULATING`: only the recirculation fan is running at `Recirculation fan start speed`
  - Activated from `IDLE` when the bed temperature rises above `RFSBT`
  - When `RECIRCULATING` is activated, the `Mode decision time` (`MDT`) timer starts
- `HEATING`: fans run according to the hot chamber settings
  - Activated from `RECIRCULATING` when `MDT` has passed and the bed temperature is above `Hot chamber bed temp threshold`
  - Remains active until the bed temperature falls below `RFSBT`, then the controller returns to `IDLE`
- `COOLING`: fans run according to the cold chamber settings
  - Activated from `RECIRCULATING` if the conditions for `HEATING` are not fulfilled
  - Remains active until the bed temperature falls below `RFSBT`, then the controller returns to `IDLE`

- `MANUAL`: all fans run at user-configured manual speeds
  - Activated when operating mode is set to `MANUAL`
  - Suspends the automatic state machine
  - Returns to `IDLE` when operating mode is changed away from `MANUAL`

State order (automatic modes):

`IDLE` -> `RECIRCULATING` -> `HEATING` or `COOLING` -> `IDLE`

`MANUAL` mode bypasses this sequence entirely.


## FAN CONTROL
- The cooling fan should be run with a PID controller and regulated with the chamber temperature sensor
- The PID setpoint is `Max cold chamber temperature`
- The PID controls only the exhaust fan during `COOLING`
- PID output is clamped between `Exhaust fan min speed %` and `Exhaust fan max %`
- In `HEATING`, the exhaust fan runs at the fixed configured hot chamber speed and is not PID-controlled
- The PID loop runs every 2 seconds
- `Kp`, `Ki`, and `Kd` must be exposed in the settings menu
- If the chamber sensor fails, the exhaust fan runs at the configured `Exhaust fan max %` and an error message is displayed on the screen
- Other fans are at this point run at speeds specified in the settings

### Fan Control Notes

**4-pin fans** (standard PC PWM fan):
- Pin 1 / black: `GND`
- Pin 2 / yellow: fan supply voltage
- Pin 3 / green: tachometer / RPM output (open-collector,
  2 pulses/rev)
- Pin 4 / blue: PWM control input
- PWM frequency: `25 kHz` (21–28 kHz acceptable)
- Speed control via PWM pin; do not PWM-switch the power pin
- Tach input needs a pull-up (ESP32 internal or external resistor)

**3-pin fans** (tach + power, no PWM pin):
- Pin 1: `GND`
- Pin 2: fan supply voltage (switched via N-channel MOSFET)
- Pin 3: tachometer output
- Speed control by PWM-switching the MOSFET on the power line
- PWM frequency: configurable (default 100 Hz, set via
  Settings → 2P/3P PWM Frequency)

**2-pin fans** (power only, no tach):
- Pin 1: `GND`
- Pin 2: fan supply voltage (switched via N-channel MOSFET)
- Speed control by PWM-switching the MOSFET on the power line
- No RPM measurement possible — RPM display shows "2PIN/NA"
- PWM frequency: same configurable frequency as 3-pin fans

A common ground between the ESP32 and each fan power supply
is required.

### Operating Mode Behavior

- `AUTO`:
  - The controller starts in `IDLE`
  - When the bed temperature rises above `RFSBT`, it enters `RECIRCULATING`
  - At the moment `RECIRCULATING` is entered, the `MDT` timer starts
  - When `MDT` has passed, the controller evaluates the bed temperature and makes one selection:
    - If the bed temperature is below `RFSBT`, return to `IDLE` because printing has stopped or did not continue
    - If the bed temperature is above `Hot chamber bed temp threshold`, enter `HEATING`
    - Otherwise, enter `COOLING`
  - The selected mode stays active until the bed temperature falls below `RFSBT`, then the controller returns to `IDLE`
- `HEAT`:
  - Enters `HEATING` mode immediately when selected, bypassing `RFSBT` and `MDT`
  - After `MDT` has passed, if bed temperature is below `RFSBT`, returns to `IDLE`
- `COOL`:
  - Enters `COOLING` mode immediately when selected, bypassing `RFSBT`
    and `MDT`
  - After `MDT` has passed, if bed temperature is below `RFSBT`,
    returns to `IDLE`
- `MANUAL`:
  - Enters `MANUAL` state immediately, suspending the state machine
  - All three fans run at the manual speed percentages configured in
    each fan's settings submenu
  - Returns to `IDLE` when operating mode is changed away from `MANUAL`
  - Forced back to `AUTO` at boot

## DEBUGGING
  - when DEBUG is selected the sensor tempeatures are read from settings and not physical hardware, This allows testing without sensors connected.
  - Add debuginfo into serial so that we can test that the encoder and buttons work as expected
  - A full serial debug/control interface is available at 115200 baud for live status, settings changes, fan presence, manual override, operating mode, and log management

### Edge Case

If `RECIRCULATING` has started and the `MDT` timer is running, but the bed temperature has fallen below `RFSBT` by the time the timer ends, the controller must return to `IDLE`. This covers cases where printing was stopped before the controller completed the mode decision.

## Current Status

- Working:
  - ESP32 firmware builds and uploads successfully
  - Dual display support: ST7789 (320×240) and ST7735 (160×128
    post-rotation)
  - Resolution-dependent font/layout scaling for both displays
  - Main TcMenu-based UI with top-level live status items and
    settings submenus
  - TcMenu color theme: white text on black, blue selection highlight
  - Custom status footer with per-mode background colors (including
    orange for MANUAL)
  - Root menu opens with `Settings` selected
  - Extra hardware button mapped as a dedicated back key
  - Chamber and under-bed temperature handling, fan PWM output,
    tachometer measurement, and automatic state machine
  - Cooling exhaust fan PID control with configurable gains
  - Fan type support: 2PIN, 3PIN, and 4PIN per fan, with
    configurable 2P/3P PWM frequency (default 100 Hz)
  - Fan RPM submenu at root level; 2-pin fans display "2PIN/NA"
  - Per-fan settings submenus (Heating/Exhaust/Recirc Fan Settings)
    with fan presence, fan type, and manual speed
  - MANUAL operating mode for direct fan speed control
  - Debug sensor simulation (Settings → Manual Debug Sens Ctrl)
  - Wi-Fi credentials configured via full-screen T9 on-screen
    keyboard (Settings → Network) and persisted to EEPROM
  - SSID item displays current SSID; password shows
    "Is set" / "Not set"
  - ESP32 supports 2.4 GHz WiFi only
  - TcMenu's built-in text editor blocked for network items;
    custom T9 keyboard used instead
  - Boot forces debug mode off, manual fan control off, and
    MANUAL mode back to AUTO
  - HTTP status page at `http://<device-ip>/` with settings dump
    and log file contents
  - Print job logging to LittleFS with configurable interval
- In progress:
  - Real-hardware validation with fans and sensors connected
- Not started:
- Known bugs:
  - None currently documented in this file

## Constraints

- Framework: Arduino (via PlatformIO, `espressif32` platform)
- Libraries:
  - `tcmenu/tcMenu` — UI framework, configured in code (no Designer tool)
  - `adafruit/Adafruit GFX Library` — graphics base for TcMenu ST7735 renderer
  - `adafruit/Adafruit ST7735 and ST7789 Library` — current display driver library
  - `paulstoffregen/OneWire` — 1-Wire bus for DS18B20 sensors
  - `milesburton/DallasTemperature` — DS18B20 temperature sensor driver
  - `br3ttb/Arduino-PID-Library` — PID controller for exhaust fan
- Settings persistence: EEPROM emulation (ESP32 flash) via TcMenu's built-in
  EEPROM support. Native Preferences library support is not yet in TcMenu.
- Display/UI style preferences:
  - Dark theme, modern look
  - White text on dark background, bold text where supported

## File Map

- `src/main.cpp` — Arduino entry point (`setup()` / `loop()`), wires everything together
- `src/menu/MenuSetup.h/.cpp` — TcMenu menu structure defined in code, renderer and
  encoder initialization
- `src/fans/FanController.h/.cpp` — PWM fan control, tachometer reading
- `src/sensors/TemperatureSensors.h/.cpp` — DS18B20 reads on both 1-Wire buses
- `src/control/StateMachine.h/.cpp` — IDLE / RECIRCULATING / HEATING / COOLING logic
- `src/control/PidController.h/.cpp` — PID wrapper around Arduino-PID-Library
- `src/settings/Settings.h/.cpp` — All persisted settings, EEPROM load/save
- `src/network/WifiManager.h/.cpp` — WiFi connection management
- `src/network/StatusWebServer.h/.cpp` — HTTP status page (settings + log files)
- `src/ui/ScreenKeyboard.h/.cpp` — Full-screen T9 on-screen keyboard
- `src/debug/SerialInterface.h/.cpp` — Serial command interface
- `src/logging/Logger.h/.cpp` — Print job logging to LittleFS
- `include/` — Shared headers and pin/constant definitions
- `lib/` — Local libraries (empty for now; all deps via PlatformIO)
- `test/` — Unit tests (PlatformIO native env)
- `Resources/` — Schematics, images, reference material

## Development Notes

- Build environment: PlatformIO with `espressif32` platform, Arduino framework
- Board target: `az-delivery-devkit-v4`
- Upload speed: 921600 baud
- Serial baud rate: 115200
- TcMenu is configured entirely in C++ code — no `.emf` Designer file is used.
  Menu items are defined using TcMenu `MenuInfo` structs and `MenuItem` subclasses
  in `PROGMEM`, linked together as a singly-linked list.
- TcMenu's `taskManager.runLoop()` must be the only call in `loop()`. Do not use
  `delay()` anywhere; use `taskManager.scheduleOnce()` or
  `taskManager.scheduleFixedRate()` for timed work.
- EEPROM emulation is used for settings persistence. TcMenu's native Preferences
  library support is not yet available (pending upstream).
- Rotary encoder pin A (`GPIO32`) is interrupt-capable on ESP32, satisfying
  TcMenu's hard requirement for encoder pin A.
- `GPIO34`, `GPIO35`, `GPIO39` are input-only — used for tachometer inputs only.
- Debug mode replaces hardware sensor reads with values set in the Debug settings
  menu, allowing testing without sensors connected.
- Wi-Fi credentials are entered via the on-screen T9 keyboard
  (Settings → Network) and persisted to EEPROM.
  The ESP32 supports 2.4 GHz WiFi only.
- The main status/footer update paths are split:
  - normal read-only menu status refresh runs at `500 ms`
  - large footer refresh runs at `5 s`

## Coding style and strategy
- Always update this document if new functionality is added
- When naming file, classes, variables. Then the name consists of two parts, always start the name with the primary descrption. An example: Do not name as: TemperatureLogger but LoggerTemperature as the "Logger" word is the main defining role of the object/file.
- prioritise simplicity always, secondy separation of concerns. All software should consist of separate "services" invoked from the main logic/function.
- Add comments and use good naming
- Standard Arduino code conventions
- Exception: use camelCase or CamelCase (Java style)
- Divide into reasonably sized files by functionality
- Use C++ classes where applicable

## TODO
