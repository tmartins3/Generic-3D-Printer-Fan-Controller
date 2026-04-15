# Generic 3D Printer Fan Controller

An ESP32-based fan controller for enclosed 3D printers. Automatically manages
heating, exhaust, and recirculation fans based on bed and chamber temperature,
with a full on-device menu UI and persistent settings.

---

## Features

- **Automatic mode selection** — monitors bed temperature to decide whether the
  chamber needs active heating or cooling
- **PID-controlled exhaust fan** in cooling mode for precise chamber temperature
  regulation
- **Three-fan support** — heating fan, exhaust fan, and recirculation fan, each
  individually configurable and optionally absent
- **On-device UI** — TcMenu-based rotary encoder interface on a colour TFT display
- **Persistent settings** — all parameters stored in ESP32 flash, survive power cycles
- **Debug menu** — simulate sensor values, override fan speeds manually, and tune
  PID values without sensors connected
- **WiFi** *(reserved for future development)* — connects and displays IP address;
  no network API yet

---

## Hardware

### Bill of Materials

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 (az-delivery-devkit-v4 or compatible) |
| Display + encoder module | TFT display with rotary encoder and two buttons |
| Display driver | ST7789 (320×240) or ST7735 — selectable in `Config.h` |
| Exhaust fan | 24 V, 4-pin PWM radial fan |
| Recirculation fan | 24 V, 4-pin PWM radial fan |
| Heating fan | 12 V, 4-pin PWM fan (under-bed) |
| Chamber temperature sensor | DS18B20 |
| Bed temperature sensor | DS18B20 |
| Step-down converter ×2 | 24 V → 5 V (for ESP32 + display), 24 V → 12 V (for heating fan) |

> The heating fan and recirculation fan can each be marked as not present in the
> Debug menu — the controller adapts the UI and fan outputs accordingly.

### Pin Mapping

| Signal | ESP32 GPIO |
|--------|-----------|
| TFT SCL (SCLK) | GPIO18 |
| TFT SDA (MOSI) | GPIO23 |
| TFT RES | GPIO4 |
| TFT DC | GPIO16 |
| TFT CS | GPIO17 |
| TFT BLK (backlight) | GPIO21 |
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
| Chamber temp sensor (1-Wire) | GPIO19 |
| Bed temp sensor (1-Wire) | GPIO22 |

> `GPIO34`, `GPIO35`, and `GPIO39` are input-only — suitable for tachometer signals.
> Each tachometer input requires a pull-up (the ESP32 internal pull-up is sufficient).

---

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32 board connected via USB

### Clone and configure

```bash
git clone https://github.com/tmartins3/Generic-3D-Printer-Fan-Controller.git
cd Generic-3D-Printer-Fan-Controller
```

Copy the WiFi credentials template and fill in your network details:

```bash
cp include/wifi_credentials.h.example include/wifi_credentials.h
# edit include/wifi_credentials.h with your SSID and password
```

> WiFi is not currently used by the application. Credentials are only needed
> to avoid a compile error — the connection is made but no network features are
> active.

### Configure display driver

Open `include/Config.h` and set `TFT_DRIVER_TYPE` to match your display:

```cpp
#define TFT_DRIVER_TYPE  DISPLAY_DRIVER_ST7789   // or DISPLAY_DRIVER_ST7735
```

Adjust `TFT_INIT_WIDTH`, `TFT_INIT_HEIGHT`, `TFT_ROTATION`, and other display
constants in the same file to match your module.

### Build and flash

```bash
pio run -t upload
```

Serial monitor at 115200 baud:

```bash
pio device monitor
```

---

## Menu Structure

```
Root (live status — read only)
├── Operating Mode      IDLE / RECIRC / HEATING / COOLING
├── Under-bed Temp      °C
├── Chamber Temp        °C
├── Heating Fan Speed   %
├── Recirculation Fan   %
├── Exhaust Fan Speed   %
└── WiFi IP

Settings
├── Operating Mode      AUTO / HEATING / COOLING
├── Mode Decision Time  min
├── Startup Bed Temp    °C
├── Start Recirc Fan Speed  %
├── Hot Chamber Settings
│   ├── Heating Fan Speed   %
│   ├── Recirculation Fan   %
│   ├── Exhaust Fan Speed   %
│   └── Bed Threshold       °C
├── Cold Chamber Settings
│   ├── Exhaust Fan Max     %
│   ├── Exhaust Fan Min     %
│   ├── Recirculation Fan   %
│   └── Max Chamber Temp    °C
└── Debug Settings
    ├── Manual Sensor Control
    │   ├── Debug Mode          ON/OFF
    │   ├── Debug Chamber Temp  °C
    │   └── Debug Bed Temp      °C
    ├── Manual Fan Control
    │   ├── Manual Control      ON/OFF
    │   ├── Heating Fan Speed   %
    │   ├── Exhaust Fan Speed   %
    │   └── Recirculation Fan   %
    ├── Cooling PID
    │   ├── PID Kp
    │   ├── PID Ki
    │   └── PID Kd
    ├── Heating Fan Present     YES/NO
    ├── Exhaust Fan Present     YES/NO
    └── Recirc Fan Present      YES/NO
```

Items are automatically hidden when the corresponding fan is marked as not present.

---

## Control Logic

The controller runs a state machine with four states:

```
IDLE ──► RECIRCULATING ──► HEATING ──► IDLE
                      └──► COOLING ──► IDLE
```

| State | Condition to enter | Fan behaviour |
|-------|--------------------|---------------|
| **IDLE** | Bed temp below startup threshold | All fans off |
| **RECIRCULATING** | Bed temp rises above startup threshold | Recirculation fan at start speed; Mode Decision Timer starts |
| **HEATING** | MDT elapsed and bed temp ≥ hot threshold | Fans at hot chamber settings |
| **COOLING** | MDT elapsed and bed temp < hot threshold | Exhaust fan PID-controlled; recirculation fan at cold setting |

All states return to **IDLE** when bed temperature falls below the startup threshold.

**Forced modes** (`HEATING` / `COOLING` selected manually): skip the recirculation
phase and enter the selected mode immediately. Return to IDLE after MDT if the bed
has cooled.

### PID Controller

The exhaust fan in COOLING mode is regulated by a PID controller targeting
`Max Chamber Temp`. Output is clamped between `Exhaust Fan Min` and
`Exhaust Fan Max`. The loop runs every 2 seconds. Gains are tunable in
`Debug Settings → Cooling PID`.

---

## Default Settings

| Parameter | Default |
|-----------|---------|
| Operating mode | AUTO |
| Mode decision time | 10 min |
| Startup bed temp | 45 °C |
| Start recirc fan speed | 30 % |
| Hot — heating fan speed | 50 % |
| Hot — recirc fan speed | 50 % |
| Hot — exhaust fan speed | 15 % |
| Hot — bed threshold | 60 °C |
| Cold — exhaust max | 100 % |
| Cold — exhaust min | 15 % |
| Cold — recirc speed | 20 % |
| Cold — max chamber temp | 38 °C |
| PID Kp / Ki / Kd | 2.0 / 0.5 / 1.0 |

---

## Dependencies

| Library | Purpose |
|---------|---------|
| [tcMenu](https://github.com/davetcc/tcMenu) | On-device menu UI |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) | Graphics base |
| [Adafruit ST7735/ST7789](https://github.com/adafruit/Adafruit-ST7735-Library) | Display driver |
| [OneWire](https://github.com/PaulStoffregen/OneWire) | 1-Wire bus |
| [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library) | DS18B20 sensor |
| [Arduino PID Library](https://github.com/br3ttb/Arduino-PID-Library) | PID controller |

---

## License

*TODO: add license*
