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
- **Claude Sonnet 4.6** (Anthropic) — firmware co-author

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
**Settings → Debug → [Fan name] Present**. When a fan is marked as not present
the controller will not drive it and related menu items are hidden.

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

Same as forced HEATING but for cooling mode. Useful for PLA/PETG prints where
you want exhaust running immediately.

---

### Manual Fan Control

**Settings → Debug → Manual Fan Control → Manual Control: ON**

Overrides all automatic control. Each fan can be set to any speed (0–100 %)
independently. The state machine is suspended while manual control is active.
Useful for testing fan wiring, benchmarking noise, or temporarily forcing
airflow without changing operating mode.

Manual control is **forced off at every boot** to prevent accidentally leaving
fans in a stuck state after a reboot.

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

**Logging interval** is configured under **Settings → Debug → Logging
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

The device IP is displayed on the menu screen under **Settings → Debug →
WiFi IP** and printed to serial at boot.

No additional libraries are required — the web server uses the built-in
ESP32 `WebServer` library.

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
| Exhaust/cooling fan | 24 V, 4-pin PWM | Radial blower recommended; add a filter for particle capture |
| Recirculation fan | 24 V, 4-pin PWM | Radial blower with HEPA/carbon filter recommended |
| Heating fan | 12 V, 4-pin PWM | Axial or radial fan mounted below or beside the print bed |
| Chamber LED strip | 24 V LED strip | Switched via N-channel MOSFET |
| N-channel MOSFET | Logic-level, e.g. IRLZ44N | Vgs(th) ≤ 4.5 V; works at 3.3 V |
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

#### 4-Pin PWM Fan Pinout

Standard PC fan 4-pin connector, looking into the fan header:

| Pin | Colour | Signal |
|-----|--------|--------|
| 1 | Black | GND |
| 2 | Yellow | Fan supply voltage (12 V or 24 V) |
| 3 | Green | Tachometer output (open-collector, 2 pulses/rev) |
| 4 | Blue | PWM input (25 kHz, 3.3 V logic is accepted by most fans) |

> Speed control is done via the PWM pin (pin 4). Do **not** PWM-switch the
> supply voltage (pin 2) — this can damage the fan motor.
>
> The tachometer output (pin 3) is open-collector. Connect a 4.7 kΩ pull-up
> resistor from the tach pin to 3.3 V. The ESP32 internal pull-up alone is
> sufficient in most cases and no external resistor is needed.

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

WiFi credentials are entered on the device itself using the on-screen
keyboard. Navigate to **Settings → Debug → Network** and edit the SSID
and password. Credentials are saved to flash and persist across reboots.

> A `wifi_credentials.h` file is no longer required. If you previously
> used one, the device will prompt for new credentials after this update
> (EEPROM layout has changed).

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
| Operating Mode | AUTO | AUTO / HEATING / COOLING |
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

### Debug Settings

| Setting | Description |
|---------|-------------|
| Manual Sensor Control → Debug Mode | Replaces sensor readings with the values below |
| Manual Sensor Control → Debug Chamber Temp | Simulated chamber temperature |
| Manual Sensor Control → Debug Bed Temp | Simulated bed temperature |
| Manual Fan Control → Manual Control | Overrides all fan outputs |
| Manual Fan Control → Heating / Exhaust / Recirc Fan Speed | Manual fan speed % |
| Cooling PID → Kp / Ki / Kd | PID gains for exhaust fan in COOLING mode (default 2.0 / 0.5 / 1.0) |
| Heating Fan Present | Mark heating fan as installed or not |
| Exhaust Fan Present | Mark exhaust fan as installed or not |
| Recirc Fan Present | Mark recirculation fan as installed or not |
| Logging Interval | 1–60 min interval for log rows (default 5 min) |

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
| HEATING | Red | `HEATING` or `HOT` if no heating fan |
| COOLING | Blue | `COOLING` or `COOL` if no exhaust fan |

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
