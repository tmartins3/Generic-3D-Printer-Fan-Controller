# Display Driver Selector Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a single compile-time constant that switches the TFT display driver between ST7735 and ST7789 without changing the rest of the UI code.

**Architecture:** Introduce a display driver selector in `include/Config.h`, then use preprocessor selection in the menu display setup so the global TFT object and initialization path match the configured controller. Keep the renderer and TcMenu integration unchanged.

**Tech Stack:** PlatformIO, Arduino framework, Adafruit GFX, Adafruit ST7735/ST7789 library, TcMenu

---

### Task 1: Add Display Driver Selector

**Files:**
- Modify: `include/Config.h`
- Modify: `src/menu/MenuSetup.cpp`

- [ ] **Step 1: Add compile-time driver constants in `include/Config.h`**

```cpp
#define DISPLAY_DRIVER_ST7735 1
#define DISPLAY_DRIVER_ST7789 2

#define TFT_DRIVER_TYPE DISPLAY_DRIVER_ST7735
```

- [ ] **Step 2: Use the selector when instantiating and initializing the display in `src/menu/MenuSetup.cpp`**

```cpp
#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7735
#include <Adafruit_ST7735.h>
static Adafruit_ST7735 gfx(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RES);
#elif TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
#include <Adafruit_ST7789.h>
static Adafruit_ST7789 gfx(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RES);
#else
#error Unsupported TFT_DRIVER_TYPE
#endif
```

```cpp
#if TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7735
    gfx.initR(INITR_BLACKTAB);
#elif TFT_DRIVER_TYPE == DISPLAY_DRIVER_ST7789
    gfx.init(TFT_WIDTH, TFT_HEIGHT);
#endif
```

- [ ] **Step 3: Keep the rest of the menu code unchanged**

```cpp
static AdafruitDrawable gfxDrawable(&gfx, 40);
GraphicsDeviceRenderer renderer(30, applicationInfo.name, &gfxDrawable);
```

### Task 2: Verify Build

**Files:**
- Verify: `platformio.ini`

- [ ] **Step 1: Run the PlatformIO build**

Run: `pio run`
Expected: Build completes successfully with exit code 0.
