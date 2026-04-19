#pragma once

// ---------------------------------------------------------------------------
// Config.h
// Hardware pin assignments and compile-time constants for FanController2.
// All GPIO numbers refer to the az-delivery-devkit-v4 (ESP32).
// ---------------------------------------------------------------------------

// --- TFT Display (ST7735s, SPI) -------------------------------------------
#define DISPLAY_DRIVER_ST7735 1
#define DISPLAY_DRIVER_ST7789 2

// Select the active TFT controller here.
#define TFT_DRIVER_TYPE DISPLAY_DRIVER_ST7735

#define PIN_TFT_SCL   18   // SPI clock (hardware SPI SCLK)
#define PIN_TFT_SDA   23   // SPI MOSI  (hardware SPI MOSI)
#define PIN_TFT_RES    4   // Reset
#define PIN_TFT_DC    16   // Data/Command
#define PIN_TFT_CS    17   // Chip select
#define PIN_TFT_BLK   21   // Backlight (GPIO HIGH = on)

#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7735
#define TFT_WIDTH     160   // post-rotation (rotation 1: 128×160 → 160 wide × 128 tall)
#define TFT_HEIGHT    128
#define TFT_INIT_WIDTH  128
#define TFT_INIT_HEIGHT 160
#define TFT_ROTATION     1
#elif TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
#define TFT_WIDTH     320
#define TFT_HEIGHT    240
#define TFT_INIT_WIDTH  240
#define TFT_INIT_HEIGHT 320
#define TFT_ROTATION     3
#else
#error Unsupported TFT_DRIVER_TYPE
#endif

// Reserve space at the bottom of the screen that TcMenu should not render into.
#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
#define TFT_RESERVED_BOTTOM_PX 80
#elif TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7735
#define TFT_RESERVED_BOTTOM_PX 30
#endif
#define TFT_MENU_HEIGHT (TFT_HEIGHT - TFT_RESERVED_BOTTOM_PX)

// --- Rotary Encoder + Buttons ------------------------------------------------
#define PIN_ENC_A     32   // Encoder channel A (interrupt-capable, required by TcMenu)
#define PIN_ENC_B     33   // Encoder channel B
#define PIN_ENC_BTN   25   // Encoder push button
#define PIN_BTN_K0    26   // Extra button K0 (currently unused)

// --- Fans (PWM output + tachometer input) ------------------------------------
#define PIN_EXHAUST_PWM    27
#define PIN_EXHAUST_TACH   34   // Input-only GPIO

#define PIN_RECIRC_PWM     14
#define PIN_RECIRC_TACH    35   // Input-only GPIO

#define PIN_HEATING_PWM    13
#define PIN_HEATING_TACH   39   // Input-only GPIO

// 4-pin PC fan PWM: target 25 kHz, acceptable 21–28 kHz.
// ESP32 LEDC base clock is 80 MHz.
// Resolution 11 bits (2047 steps) -> 80 000 000 / (2^11) = ~39 kHz  (too high)
// Resolution 12 bits (4095 steps) -> 80 000 000 / (2^12) = ~19.5 kHz (too low)
// Use resolution 11 bits with a divider: freq = 80MHz / prescaler / 2^res
// Simplest: use ledc with freq=25000 and let ESP-IDF pick resolution automatically.
#define FAN_PWM_FREQ_HZ    25000
#define FAN_PWM_RESOLUTION 11      // bits — gives ~39 kHz at 80 MHz, nearest fit
                                   // PlatformIO/Arduino ledcSetup accepts explicit freq,
                                   // actual output will be closest achievable.
#define FAN_PWM_MAX_DUTY   ((1 << FAN_PWM_RESOLUTION) - 1)  // 2047

// LEDC channels (ESP32 has 16 channels, 0–15)
#define LEDC_CH_EXHAUST    0
#define LEDC_CH_RECIRC     1
#define LEDC_CH_HEATING    2

// --- Temperature Sensors (DS18B20, one per 1-Wire bus) ----------------------
#define PIN_TEMP_CHAMBER   19
#define PIN_TEMP_BED       22

// DS18B20 resolution: 12-bit (~750 ms conversion time)
#define TEMP_RESOLUTION_BITS 12

// Sentinel value returned when a sensor read fails
#define TEMP_READ_ERROR    -127.0f

// --- Chamber Light (24V LED strip via N-channel logic-level MOSFET) ----------
// GPIO 5 → 100 Ω → MOSFET gate. Add 10 kΩ gate-to-GND pull-down on the PCB.
#define PIN_CHAMBER_LIGHT   5

// --- Serial Debug ------------------------------------------------------------
#define SERIAL_BAUD        115200

// --- Timing ------------------------------------------------------------------
// PID loop interval (ms)
#define PID_INTERVAL_MS    2000

// Temperature sensor read interval (ms)
#define TEMP_READ_INTERVAL_MS  2000

// Tachometer measurement window (ms)
#define TACH_MEASURE_MS    1000

// Fan speed update interval (ms)
#define FAN_UPDATE_INTERVAL_MS  500
