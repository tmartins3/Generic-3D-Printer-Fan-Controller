# Menu Label Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand menu item labels to longer, clearer single-line text while preserving fit on the current 320x240 ST7789 layout with 2x text magnification.

**Architecture:** Estimate the available label width from the current renderer configuration, reserve space for right-aligned values, then update each menu label in `src/menu/MenuSetup.cpp` to the longest safe wording supported by `PROJECT.md`.

**Tech Stack:** PlatformIO, Arduino framework, TcMenu, Adafruit GFX

---

### Task 1: Calculate Safe Label Budget

**Files:**
- Read: `src/menu/MenuSetup.cpp`
- Read: `include/Config.h`
- Read: `PROJECT.md`

- [ ] **Step 1: Determine the current display and font constraints**

Use the current configuration:

```cpp
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
```

```cpp
themeBuilder.defaultItemProperties()
    .withNativeFont(nullptr, 2)
```

- [ ] **Step 2: Reserve width for the right-aligned value and choose longest safe labels**

Use the project document wording where possible, but keep every label on one line.

### Task 2: Patch Labels And Verify

**Files:**
- Modify: `src/menu/MenuSetup.cpp`

- [ ] **Step 1: Replace abbreviated labels with longer safe versions**

Examples:

```cpp
"Decision T" -> "Decision Time"
"Recirc Spd" -> "Recirculation"
"Bed Thresh" -> "Bed Threshold"
```

- [ ] **Step 2: Run the PlatformIO build**

Run: `pio run`
Expected: Build completes successfully with exit code 0.
