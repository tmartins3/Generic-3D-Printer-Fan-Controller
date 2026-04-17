#pragma once

// ---------------------------------------------------------------------------
// ScreenKeyboard.h
// Full-screen on-screen keyboard for the ST7789 320x240 display.
//
// Usage:
//   ScreenKeyboard::activate("WiFi SSID:", buf, sizeof(buf), [](bool ok) {
//       if (ok) Serial.println(buf);
//   });
//
// The keyboard takes over the TcMenu display via renderer.takeOverDisplay().
// The rotary encoder navigates among the 18 keys (6 cols × 3 rows).
// Short press = confirm focused key. Long press = cancel.
//
// T9-style input: pressing a multi-char key repeatedly within 1 s cycles
// through its characters. A different key press or a 1 s timeout flushes
// the pending character to the buffer.
//
// Layouts: 0 = lowercase, 1 = uppercase, 2 = numbers/symbols.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <BaseRenderers.h>   // RenderPressMode

typedef void (*KeyboardDoneCallback)(bool accepted);

class ScreenKeyboard {
public:
    // Activate the keyboard.
    //   prompt  — label shown in header
    //   buf     — output buffer (pre-fill for editing an existing value)
    //   bufSize — capacity including null terminator
    //   onDone  — called when the user presses OK (true) or ESC/long-press (false)
    static void activate(const char* prompt,
                         char*       buf,
                         size_t      bufSize,
                         KeyboardDoneCallback onDone);

    static bool isActive() { return _active; }

private:
    // TcMenu takeOverDisplay callback
    static void _renderCb(unsigned int encoderVal, RenderPressMode pressMode);

    // Drawing helpers
    static void _drawAll();
    static void _drawHeader();
    static void _drawButton(uint8_t idx, bool focused);

    // Input handlers
    static void _pressButton(uint8_t idx);
    static void _appendChar(char c);
    static void _flushEditChar();
    static void _setLayout(uint8_t layoutIdx);
    static void _finish(bool accepted);

    // State
    static bool     _active;
    static char*    _buf;
    static size_t   _bufSize;
    static const char* _prompt;
    static KeyboardDoneCallback _onDone;
    static uint8_t  _layoutIdx;     // 0=lower, 1=upper, 2=numbers
    static uint8_t  _editBtnIdx;    // 0xFF = no pending T9 char
    static uint8_t  _editCharIdx;   // position within that button's chars
    static uint32_t _lastEditMs;    // millis() of last T9 press
    static uint8_t  _lastCommitBtn; // last button committed by timeout (block re-press)
    static uint8_t  _focused;       // currently highlighted button (0..17)
    static uint8_t  _prevFocused;   // previous focused button for partial redraw
    static bool     _needFullRedraw;  // full screen redraw (layout change)
    static bool     _needHeaderRedraw;
    static bool     _needFocusRedraw; // only focused/unfocused buttons changed
    static bool     _prevBtnDown;   // previous button state for edge detection
    static uint32_t _btnDownMs;     // millis() when button went down (for held detection)
    static bool     _pendingFinish; // waiting for buttons to release before giveBackDisplay
    static bool     _finishAccepted;// true=OK, false=ESC/cancel
};
