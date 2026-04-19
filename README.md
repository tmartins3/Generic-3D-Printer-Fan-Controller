# Generic 3D Printer Fan Controller

An ESP32-based automatic fan controller for enclosed 3D printers. Monitors bed
and chamber temperature and automatically manages up to three fans — a heating
fan, an exhaust/cooling fan, and a recirculation fan — to keep the enclosure at
the right temperature during and after a print.

Any combination of one to three fans can be installed. The controller also
supports a **24 V LED chamber light** switched via a MOSFET on GPIO 5. The
controller adapts its behaviour and hides irrelevant menu items based on which
hardware is configured as present.

---

## Authors

- **Tomas Martinsen** — hardware design, requirements, and real-world testing
- **Claude Sonnet 4.6 / Opus 4.6** (Anthropic) — firmware co-author

---

## How It Works

### The Problem It Solves

Enclosed 3D printers benefit from temperature management in two opposite
directions depending on the filament being printed:

- **High-temperature filaments** (ABS, ASA, PC, Nylon) need a *warm* chamber to
  prevent warping and layer delamination. A heating fan moves heat from the warm
  print bed up into the chamber air.
- **Low-temperature filaments** (PLA, PETG) print better in a *cooler* chamber.
  An exhaust fan removes hot air and replaces it with cooler room air.
- A **recirculation fan** keeps the chamber air moving evenly regardless of mode,
  which improves temperature uniformity and filter efficiency.

This controller decides which fans to run, and at what speed, automatically —
based on the bed temperature as a proxy for what the printer is doing.

---

### The Three Fans

| Fan | Role | Notes |
|-----|------|-------|
| **Heating fan** | Moves heat from under the print bed up into the chamber | Typically a 12 V fan mounted below the bed; off in cooling mode |
| **Exhaust fan** | Removes hot air from the chamber; regulates chamber temperature via PID in cooling mode | Typically a 24 V radial fan, often with a filter |
| **Recirculation fan** | Circulates air inside the chamber without venting outside | Typically a 24 V radial fan with a HEPA/carbon filter |

Any of the three can be omitted. Enable or disable each fan under
**Settings → [Fan name] Fan Settings → [Fan] Present**. When a fan is
marked as not present the controller will not drive it and related menu
items are hidden.

Each fan can be configured as **4-pin** (standard PWM + tach), **3-pin**
(tach only, MOSFET speed control), or **2-pin** (no tach, MOSFET speed
control). Fan type is set per fan under Settings → [Fan] Fan Settings →
Fan Type. 2-pin and 3-pin fans use a configurable lower PWM frequency
(default 100 Hz, set under Settings → 2P/3P PWM Frequency). 2-pin fans
cannot report RPM and show "2PIN/NA" in the RPM submenu.

Each fan also has an **Invert PWM** option (default: NO). When enabled,
the PWM duty cycle is inverted (0% duty = full speed, 100% duty = off).
This allows driving fans through a simple NPN transistor + P-channel
MOSFET circuit when logic-level N-channel MOSFETs are not available.

---

### Control Modes

The operating mode is selected under **Settings → Operating Mode**.

#### AUTO (recommended)

The controller observes the bed temperature and works through a fixed sequence:

```
IDLE ──► RECIRCULATING ──► HEATING ──► IDLE
                      └──► COOLING ──► IDLE
```

**IDLE** — All fans off. The printer is idle or the bed has cooled down.
Entered when bed temperature is below *Startup Bed Temp* (default 45 °C).

**RECIRCULATING** — Only the recirculation fan runs, at *Start Recirc Fan Speed*.
Entered when bed temperature rises above *Startup Bed Temp*. At this point
the controller cannot yet tell whether this will be a hot-chamber or
cold-chamber print, so it starts air circulation and begins timing the
*Mode Decision Time* (MDT) countdown.

**HEATING** — Heating fan, recirculation fan, and exhaust fan all run at the
speeds configured under *Hot Chamber Settings*. Entered after MDT has elapsed
if the bed temperature is at or above *Bed Threshold* (default 60 °C), meaning
the printer is using a high-temperature material.

**COOLING** — Recirculation fan runs at the cold-chamber setting. Exhaust fan
is PID-controlled to hold chamber temperature at *Max Chamber Temp* (default
38 °C). Heating fan is off. Entered after MDT has elapsed if bed temperature
is below *Bed Threshold*, meaning the printer is using a low-temperature
material.

Both HEATING and COOLING return to **IDLE** when bed temperature falls below
*Startup Bed Temp*, indicating the print has finished and the bed has cooled.

> **Mode Decision Time** gives the bed time to stabilise before the controller
> commits to a mode. A 10-minute default works well for most printers; adjust
> it under Settings if your bed heats slowly.

#### HEATING (forced)

Enters HEATING mode immediately without waiting for the bed to warm up or
the MDT to elapse. Useful when you know you are printing a high-temperature
material and want fans on from the start.
Returns to IDLE after MDT has elapsed *and* bed temperature has fallen below
*Startup Bed Temp*.

#### COOLING (forced)

Same as forced HEATING but for cooling mode. Useful for PLA/PETG prints
where you want exhaust running immediately.

#### MANUAL

Overrides all automatic control. Each fan runs at the manual speed
percentage configured in its fan settings submenu (Settings → [Fan]
Fan Settings → Manual Speed). The state machine is suspended while
MANUAL mode is active. Useful for testing fan wiring, benchmarking
noise, or temporarily forcing airflow.

MANUAL mode is **reset to AUTO at every boot** to prevent accidentally
leaving fans in a stuck state after a reboot.

---

### Print Job Logging

The controller logs temperature and fan data for each print job to the
onboard flash (LittleFS).

**Two log files are kept** — one for hot-chamber jobs, one for cold-chamber
jobs. Each new completed job overwrites the previous log of the same type.

**Log file names:**
- `HOT.log` — last HEATING mode job
- `COLD.log` — last COOLING mode job

**Job lifecycle:**
- Logging starts automatically when the state machine enters RECIRCULATING
  (AUTO mode) or when the user forces HEATING or COOLING mode.
- Logging ends when the state machine returns to IDLE.
- If the device loses power mid-print the incomplete log is discarded on
  the next boot.

**Log format:**

```
# FanController2 Print Log
# Started: 4 min since boot
# Op Mode: AUTO
# MDT: 10 min
# ...all active settings...
#
# min;mode;bedC;chamberC;recircRPM;exhaustRPM;heatingRPM
0;RECIRC;47.2;24.1;850;0;0
10;HEATING;68.1;35.2;1200;300;1800
```

**Logging interval** is configured under **Settings → Logging
Interval** (1–60 min, default 5 min).

**Maximum file size** is 128 KB per file (~52 hours at 1-minute intervals).
When the limit is reached logging stops and a truncation marker is written.

**Retrieving logs via serial monitor** (115200 baud):

| Command | Action |
|---------|--------|
| `log list` | List files with size and row count |
| `log fetch hot` | Stream HOT.log to serial |
| `log fetch cold` | Stream COLD.log to serial |
| `log fetch active` | Stream in-progress log |
| `log delete hot` | Delete HOT.log |
| `log delete cold` | Delete COLD.log |

---

### Web Status Page

When WiFi is connected, the controller serves a plain-text status page at
`http://<device-ip>/` (port 80). The page contains:

- All current settings (operating mode, fan speeds, PID values, etc.)
- The contents of HOT.log (last hot-chamber print)
- The contents of COLD.log (last cold-chamber print)

If a log file does not exist, `LOGFILE NOT PRESENT` is shown in its place.

The device IP is displayed on the menu screen under **Settings →
Network → WiFi IP** and printed to serial at boot.

No additional libraries are required — the web server uses the built-in
ESP32 `WebServer` library.

---

### Serial Debug Interface

A full interactive command interface is available over serial (115200 baud).
Connect with any serial terminal or `pio device monitor`.

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `status` | Live state, temperatures, fan speeds, and flags |
| `get` | Dump all current settings |
| `set debug on\|off` | Enable/disable simulated sensor mode |
| `set chamber <°C>` | Set simulated chamber temperature |
| `set bed <°C>` | Set simulated bed temperature |
| `set manual on\|off` | Enable/disable manual fan override |
| `set heating <0-100>` | Manual heating fan speed % |
| `set exhaust <0-100>` | Manual exhaust fan speed % |
| `set recirc <0-100>` | Manual recirc fan speed % |
| `set mode auto\|heating\|cooling` | Set operating mode |
| `set mdt <min>` | Set mode decision time |
| `set rfsbt <°C>` | Set recirculation start bed temperature |
| `set threshold <°C>` | Set hot chamber bed threshold |
| `set hfp on\|off` | Set heating fan present |
| `set efp on\|off` | Set exhaust fan present |
| `set rfp on\|off` | Set recirc fan present |
| `set hip on\|off` | Invert heating fan PWM signal |
| `set eip on\|off` | Invert exhaust fan PWM signal |
| `set rip on\|off` | Invert recirc fan PWM signal |
| `set light on\|off` | Toggle chamber light |
| `log list` | List log files with sizes |
| `log fetch hot\|cold\|active` | Stream a log file to serial |
| `log delete hot\|cold` | Delete a log file |

All `set` commands persist changes immediately. The serial interface is
also used by the automated test suite (`docs/testing/test_runner.py`).

---

### Sensor Failure Handling

If the chamber temperature sensor fails while in COOLING mode the exhaust fan
runs at *Exhaust Fan Max* (100 % by default) to ensure the chamber does not
overheat. The state machine continues to operate normally in all other
respects.

---

## Hardware

### What You Need

| Component | Specification | Notes |
|-----------|--------------|-------|
| Microcontroller | ESP32 (az-delivery-devkit-v4 or pin-compatible) | Any ESP32 dev board with the same GPIO layout will work |
| Display + input module | TFT colour display with rotary encoder and 2 buttons | Common combined modules sold for Arduino/ESP32 |
| TFT controller | ST7789 (320×240) or ST7735 (128×160) | Set in `include/Config.h` |
| Exhaust/cooling fan | 24 V, 2/3/4-pin | Radial blower recommended |
| Recirculation fan | 24 V, 2/3/4-pin | With HEPA/carbon filter |
| Heating fan | 12 V, 2/3/4-pin | Below or beside the print bed |
| Chamber LED strip | 24 V LED strip | Switched via N-channel MOSFET |
| N-channel MOSFET | Logic-level, e.g. IRLZ44N | One for chamber light; one per 2/3-pin fan |
| Chamber temp sensor | DS18B20 | Waterproof probe version recommended; mount away from direct airflow |
| Bed temp sensor | DS18B20 | Mount outside the fan airflow, on the underside of the bed or frame |
| Step-down converter | 24 V → 5 V, ≥1 A | Powers ESP32 and display |
| Step-down converter | 24 V → 12 V, rated for heating fan current | Powers the heating fan |
| Pull-up resistors | 4.7 kΩ | One per DS18B20 1-Wire bus, between data and 3.3 V |
| Fan power supply | 24 V DC, sufficient amperage for fans and LED strip | |

> All hardware except the microcontroller, display, and at least one sensor is
> optional. The controller works with any combination of fans and with or without
> the chamber light.

---

### Wiring

#### Power

```
24 V supply ──► 24 V → 5 V converter ──► ESP32 5V + display VCC
            ──► 24 V → 12 V converter ──► heating fan supply (pin 2)
            ──► exhaust fan supply (pin 2)
            ──► recirculation fan supply (pin 2)
GND ──────────► all GND rails (ESP32, converters, fans) — common ground required
```

> A common ground between the ESP32 and all fan power supplies is essential.
> Without it, PWM and tachometer signals will not be referenced correctly.

#### ESP32 Pin Mapping

| Signal | ESP32 GPIO |
|--------|-----------|
| TFT SCLK | GPIO18 |
| TFT MOSI (SDA) | GPIO23 |
| TFT Reset | GPIO4 |
| TFT DC | GPIO16 |
| TFT CS | GPIO17 |
| TFT Backlight | GPIO21 |
| Encoder A | GPIO32 |
| Encoder B | GPIO33 |
| Encoder button | GPIO25 |
| Back button (K0) | GPIO26 |
| Exhaust fan PWM | GPIO27 |
| Exhaust fan tach | GPIO34 |
| Recirculation fan PWM | GPIO14 |
| Recirculation fan tach | GPIO35 |
| Heating fan PWM | GPIO13 |
| Heating fan tach | GPIO39 |
| Chamber sensor (1-Wire) | GPIO19 |
| Bed sensor (1-Wire) | GPIO22 |

> GPIO34, GPIO35, and GPIO39 are input-only on ESP32 — they cannot be used
> for output. This makes them ideal for tachometer inputs.

#### Fan Pinouts

**4-pin PWM fan** (standard PC fan connector):

| Pin | Colour | Signal |
|-----|--------|--------|
| 1 | Black | GND |
| 2 | Yellow | Fan supply voltage (12/24 V) |
| 3 | Green | Tachometer (open-collector, 2 pulses/rev) |
| 4 | Blue | PWM input (25 kHz) |

> Speed control via PWM pin (pin 4). Do **not** PWM-switch
> the supply voltage — this can damage the fan motor.
> Tach needs a 4.7 kΩ pull-up to 3.3 V (ESP32 internal
> pull-up is usually sufficient).

**3-pin fan** (tach + power, no dedicated PWM pin):

| Pin | Colour | Signal |
|-----|--------|--------|
| 1 | Black | GND |
| 2 | Red | Fan supply voltage (via MOSFET) |
| 3 | Yellow | Tachometer (open-collector, 2 pulses/rev) |

> Speed control by PWM-switching an N-channel MOSFET on the
> supply line. Default PWM frequency: 100 Hz (configurable
> under Settings → 2P/3P PWM Frequency).

**2-pin fan** (power only):

| Pin | Colour | Signal |
|-----|--------|--------|
| 1 | Black | GND |
| 2 | Red | Fan supply voltage (via MOSFET) |

> Same MOSFET speed control as 3-pin. No RPM measurement —
> the controller displays "2PIN/NA" for these fans.

#### Chamber Light MOSFET Wiring

An N-channel logic-level MOSFET (e.g. IRLZ34N, IRLZ44N) switches the 24 V LED
strip from the 3.3 V ESP32 GPIO.

```
                          24 V PSU (+)
                               │
                          LED strip (+)
                          LED strip (–)
                               │
                             Drain
                          ┌────┤  IRLZ34N (TO-220)
ESP32 GPIO 5 ──[100 Ω]──── Gate┤
                          └────┤
GND ──────[10 kΩ]──────── Source
GND ───────────────────── Source
                               │
                             GND (common with ESP32)
```

| MOSFET pin | Connects to |
|------------|-------------|
| Gate | ESP32 GPIO 5 via 100 Ω resistor |
| Gate | GND via 10 kΩ pull-down (keeps light off during boot) |
| Drain | LED strip negative (–) lead |
| Source | GND (must share ground with ESP32) |

LED strip positive (+) connects directly to 24 V supply.
The MOSFET source must share a common GND with the ESP32.

> **Why a pull-down on the gate?** During ESP32 boot the GPIO is floating for
> a brief moment. The 10 kΩ resistor to GND holds the gate low so the light
> does not flash on unintentionally at power-up.

> **Heat dissipation:** At typical LED strip currents (0.5–2 A) the IRLZ34N
> dissipates under 100 mW — no heatsink required.

---

#### DS18B20 Temperature Sensor Wiring

```
DS18B20 VDD ──► 3.3 V
DS18B20 GND ──► GND
DS18B20 DQ  ──► GPIO (see pin table above)
              └─ 4.7 kΩ pull-up to 3.3 V
```

Each sensor uses its own 1-Wire bus (separate GPIO). Do not share a bus
between the chamber and bed sensors.

---

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32 connected via USB

### 1. Clone the repository

```bash
git clone https://github.com/tmartins3/Generic-3D-Printer-Fan-Controller.git
cd Generic-3D-Printer-Fan-Controller
```

### 2. Configure WiFi (on device)

WiFi credentials are entered on the device itself using a full-screen
T9-style on-screen keyboard. Navigate to **Settings → Network** and
select **SSID 2.4G** or **Password**. The SSID item shows the
currently configured network name; the password item shows "Is set" or
"Not set" (the actual password is never displayed).

Credentials are saved to flash and persist across reboots.

> **Important:** The ESP32 only supports **2.4 GHz WiFi**. It cannot
> connect to 5 GHz networks. If your router uses a single SSID for both
> bands, either configure a separate 2.4 GHz SSID or ensure the ESP32
> can see the 2.4 GHz band.

### 3. Select your display driver

Open `include/Config.h` and set `TFT_DRIVER_TYPE`:

```cpp
#define TFT_DRIVER_TYPE  DISPLAY_DRIVER_ST7789   // 320×240
// or
#define TFT_DRIVER_TYPE  DISPLAY_DRIVER_ST7735   // 128×160
```

Verify the rotation and inversion settings in the same file match your
specific module.

### 4. Build and upload

```bash
pio run -t upload
```

### 5. Open the serial monitor (optional)

```bash
pio device monitor
```

Baud rate: 115200. State machine transitions, sensor readings, and fan speeds
are logged here.

---

## Settings Reference

All settings are persisted to flash immediately when changed.

### Top level

| Setting | Default | Description |
|---------|---------|-------------|
| Operating Mode | AUTO | AUTO / HEATING / COOLING / MANUAL |
| Mode Decision Time | 10 min | How long the controller recirculates before choosing HEATING or COOLING |
| Startup Bed Temp | 45 °C | Bed temperature that triggers the start of recirculation |
| Start Recirc Fan Speed | 30 % | Recirculation fan speed during the decision period |

### Hot Chamber Settings

| Setting | Default | Description |
|---------|---------|-------------|
| Heating Fan Speed | 50 % | Heating fan speed in HEATING mode |
| Recirculation Fan | 50 % | Recirc fan speed in HEATING mode |
| Exhaust Fan Speed | 15 % | Exhaust fan speed in HEATING mode (small amount to avoid pressure build-up) |
| Bed Threshold | 60 °C | Bed temperature above which HEATING is chosen over COOLING |

### Cold Chamber Settings

| Setting | Default | Description |
|---------|---------|-------------|
| Exhaust Fan Max | 100 % | PID output upper limit |
| Exhaust Fan Min | 15 % | PID output lower limit (keeps air moving at minimum) |
| Recirculation Fan | 20 % | Recirc fan speed in COOLING mode |
| Max Chamber Temp | 38 °C | PID setpoint — target chamber temperature in COOLING mode |

### Per-Fan Settings

Each fan has its own settings submenu under Settings.

**Heating Fan Settings:**

| Setting | Description |
|---------|-------------|
| Heating Fan Present | Mark as installed or not |
| Fan Type | 2PIN / 3PIN / 4PIN |
| Invert PWM | Invert duty cycle for NPN+MOSFET circuits |
| Manual Speed | Fan speed % in MANUAL mode |

**Exhaust Fan Settings:**

| Setting | Description |
|---------|-------------|
| Exhaust Fan Present | Mark as installed or not |
| Fan Type | 2PIN / 3PIN / 4PIN |
| Invert PWM | Invert duty cycle for NPN+MOSFET circuits |
| Manual Speed | Fan speed % in MANUAL mode |
| PID Kd / Ki / Kp | PID gains for COOLING mode |

**Recirc Fan Settings:**

| Setting | Description |
|---------|-------------|
| Recirc Fan Present | Mark as installed or not |
| Fan Type | 2PIN / 3PIN / 4PIN |
| Invert PWM | Invert duty cycle for NPN+MOSFET circuits |
| Manual Speed | Fan speed % in MANUAL mode |

### Other Settings

| Setting | Default | Description |
|---------|---------|-------------|
| 2P/3P PWM Frequency | 100 Hz | PWM freq for 2-pin and 3-pin fans (0–1000) |
| Logging Interval | 5 min | Log row interval (1–60 min) |
| Chamber Light Present | ON | Mark chamber light as installed |

### Manual Debug Sens Ctrl

| Setting | Description |
|---------|-------------|
| Manual Sensor Control | ON/OFF — replaces sensor reads with values below |
| Manual Chamber Temp | Simulated chamber temperature |
| Manual Bed Temp | Simulated bed temperature |

### Network

| Setting | Description |
|---------|-------------|
| SSID 2.4G | Current WiFi SSID (T9 keyboard) |
| Password | "Is set" / "Not set" (T9 keyboard) |
| WiFi IP | Current IP address (read-only) |

---

## On-Screen Keyboard

WiFi credentials are entered using a full-screen T9-style keyboard that
takes over the display. The keyboard has three layouts (lowercase,
uppercase, numbers/symbols) arranged as a 6×3 grid of buttons.

- **Rotary encoder** navigates between buttons
- **Encoder press** selects a character; repeated presses within the
  700 ms timeout cycle through the characters on that button
  (e.g. a → b → c → a → …)
- **Pending character** is shown in bright yellow; committed characters
  are shown in orange
- **Timeout expiry** commits the pending character automatically
- **Back button (K0)** cancels and exits without saving
- **Long encoder press** (≥600 ms) also cancels
- **On-screen OK** saves and triggers WiFi reconnection (shows
  "Setting up WiFi…" message)
- **On-screen ESC** cancels without saving

> The keyboard reads the encoder button directly via GPIO because
> TcMenu's `takeOverDisplay` callback does not provide individual
> press edges. TcMenu's built-in text editor is suppressed for
> network menu items via `MenuManagerObserver::menuEditStarting()`.

---

## On-Screen Display

The screen is split into two areas:

**Menu area (top)** — standard TcMenu interface, dark theme with white text
and blue selection highlight. Scroll with the rotary encoder; press to edit;
press the back button (K0) to go up a level.

**Status footer (bottom, 80 px)** — always visible, colour-coded by mode:

| Mode | Footer colour | Label |
|------|--------------|-------|
| IDLE | Dark grey | `IDLE` |
| RECIRCULATING | Teal | `RECIRC` |
| HEATING | Red | `HEATING` or `HOT` |
| COOLING | Blue | `COOLING` or `COOL` |
| MANUAL | Orange | `MANUAL` |

The footer shows the current mode label and chamber temperature, for example:
`COOLING 34 C`

---

## Dependencies

| Library | Purpose |
|---------|---------|
| [tcMenu](https://github.com/davetcc/tcMenu) | On-device menu UI with rotary encoder |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) | 2D graphics base layer |
| [Adafruit ST7735/ST7789](https://github.com/adafruit/Adafruit-ST7735-Library) | TFT display driver |
| [OneWire](https://github.com/PaulStoffregen/OneWire) | 1-Wire bus driver |
| [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library) | DS18B20 temperature sensor |
| [Arduino PID Library](https://github.com/br3ttb/Arduino-PID-Library) | PID controller for exhaust fan |

---

## License

MIT License — see [LICENSE](LICENSE) for details.
