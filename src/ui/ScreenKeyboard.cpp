#include "ScreenKeyboard.h"
#include "../../include/Config.h"
#include "../menu/MenuSetup.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <tcMenu.h>

// ---------------------------------------------------------------------------
// ScreenKeyboard.cpp
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Layout definitions (6 cols × 3 rows = 18 buttons per layout)
//
// Grid order: left-to-right, top-to-bottom
//   idx 0-5  = row 0 (top)
//   idx 6-11 = row 1 (middle)
//   idx 12-17= row 2 (bottom)
//
// chars == "" means the key is a special/action key identified by its label.
// ---------------------------------------------------------------------------

#define KB_LAYOUTS 3
#define KB_COLS    6
#define KB_ROWS    3
#define KB_COUNT   18   // KB_COLS * KB_ROWS

struct KeyDef {
    const char* label;
    const char* chars;   // "" for special action keys
};

static const KeyDef _layouts[KB_LAYOUTS][KB_COUNT] = {
    // -----------------------------------------------------------------------
    // Layout 0 — lowercase
    // -----------------------------------------------------------------------
    {
    // row 0
    { "123",  ""     },   //  0  → layout 2
    { ".,!",  ".,!"  },   //  1  . , !
    { "abc",  "abc"  },   //  2
    { "def",  "def"  },   //  3
    { "-_@",  "-_@"  },   //  4
    { "DEL",  ""     },   //  5
    // row 1
    { "ABC",  ""     },   //  6  → layout 1
    { "ghi",  "ghi"  },   //  7
    { "jkl",  "jkl"  },   //  8
    { "mno",  "mno"  },   //  9
    { "$#&",  "$#&"  },   // 10
    { "CLR",  ""     },   // 11
    // row 2
    { "ESC",  ""     },   // 12
    { "pqrs", "pqrs" },   // 13
    { "tuv",  "tuv"  },   // 14
    { "wxyz", "wxyz" },   // 15
    { "SPC",  ""     },   // 16
    { "OK",   ""     },   // 17
    },

    // -----------------------------------------------------------------------
    // Layout 1 — uppercase
    // -----------------------------------------------------------------------
    {
    // row 0
    { "123",  ""     },   //  0  → layout 2
    { ".,!",  ".,!"  },   //  1
    { "ABC",  "ABC"  },   //  2
    { "DEF",  "DEF"  },   //  3
    { "-_@",  "-_@"  },   //  4
    { "DEL",  ""     },   //  5
    // row 1
    { "abc",  ""     },   //  6  → layout 0
    { "GHI",  "GHI"  },   //  7
    { "JKL",  "JKL"  },   //  8
    { "MNO",  "MNO"  },   //  9
    { "$#&",  "$#&"  },   // 10
    { "CLR",  ""     },   // 11
    // row 2
    { "ESC",  ""     },   // 12
    { "PQRS", "PQRS" },   // 13
    { "TUV",  "TUV"  },   // 14
    { "WXYZ", "WXYZ" },   // 15
    { "SPC",  ""     },   // 16
    { "OK",   ""     },   // 17
    },

    // -----------------------------------------------------------------------
    // Layout 2 — numbers / symbols
    // -----------------------------------------------------------------------
    {
    // row 0
    { "abc",  ""     },   //  0  → layout 0
    { "7",    "7"    },   //  1
    { "8",    "8"    },   //  2
    { "9",    "9"    },   //  3
    { "-+/",  "-+/"  },   //  4
    { "DEL",  ""     },   //  5
    // row 1
    { "ABC",  ""     },   //  6  → layout 1
    { "4",    "4"    },   //  7
    { "5",    "5"    },   //  8
    { "6",    "6"    },   //  9
    { ".,@",  ".,@"  },   // 10
    { "CLR",  ""     },   // 11
    // row 2
    { "ESC",  ""     },   // 12
    { "1",    "1"    },   // 13
    { "2",    "2"    },   // 14
    { "3",    "3"    },   // 15
    { "0",    "0"    },   // 16
    { "OK",   ""     },   // 17
    },
};

// ---------------------------------------------------------------------------
// Screen geometry
// 320 × 240 total display
// Header: y = 0..KB_HDR_H-1
// Keyboard: y = KB_HDR_H..239
// ---------------------------------------------------------------------------

#define KB_HDR_H    50                              // header height (px)
#define KB_KB_H     (TFT_HEIGHT - KB_HDR_H)        // 190
#define KB_CELL_W   (TFT_WIDTH  / KB_COLS)         // 53
#define KB_CELL_H   (KB_KB_H   / KB_ROWS)          // 63
#define KB_PAD      3
#define KB_BTN_W    (KB_CELL_W - 2 * KB_PAD)       // 47
#define KB_BTN_H    (KB_CELL_H - 2 * KB_PAD)       // 57

// Colors (RGB565)
#define COL_BG       0x0000u   // black
#define COL_BTN_NORM 0x2945u   // dark gray   RGB(40,40,40)
#define COL_BTN_SPL  0x4208u   // mid gray    RGB(64,64,64)  — special/action keys
#define COL_BTN_FOC  0x0296u   // blue        RGB(0,80,180) — TcMenu theme
#define COL_BORDER   0x39E7u   // light gray border
#define COL_TEXT     0xFFFFu   // white
#define COL_INPUT    0xFD20u   // orange  — typed chars
#define COL_PEND     0xFFE0u   // yellow  — T9 pending char
#define COL_PROMPT   0x7BEFu   // light gray — prompt label
#define COL_CURSOR   0xFFFFu   // cursor bar

// T9 timeout (ms)
#define T9_TIMEOUT_MS 700

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

bool     ScreenKeyboard::_active      = false;
char*    ScreenKeyboard::_buf         = nullptr;
size_t   ScreenKeyboard::_bufSize     = 0;
const char* ScreenKeyboard::_prompt   = nullptr;
KeyboardDoneCallback ScreenKeyboard::_onDone = nullptr;
uint8_t  ScreenKeyboard::_layoutIdx   = 0;
uint8_t  ScreenKeyboard::_editBtnIdx  = 0xFF;
uint8_t  ScreenKeyboard::_editCharIdx = 0;
uint32_t ScreenKeyboard::_lastEditMs  = 0;
uint8_t  ScreenKeyboard::_lastCommitBtn = 0xFF;
uint8_t  ScreenKeyboard::_focused     = 0;
uint8_t  ScreenKeyboard::_prevFocused = 0xFF;
bool     ScreenKeyboard::_needFullRedraw  = true;
bool     ScreenKeyboard::_needHeaderRedraw = false;
bool     ScreenKeyboard::_needFocusRedraw  = false;
bool     ScreenKeyboard::_prevBtnDown     = false;
uint32_t ScreenKeyboard::_btnDownMs      = 0;
bool     ScreenKeyboard::_pendingFinish  = false;
bool     ScreenKeyboard::_finishAccepted = false;

// ---------------------------------------------------------------------------
// activate — entry point
// ---------------------------------------------------------------------------

void ScreenKeyboard::activate(const char*          prompt,
                               char*                buf,
                               size_t               bufSize,
                               KeyboardDoneCallback onDone) {
    if (_active) return;

    _prompt      = prompt ? prompt : "";
    _buf         = buf;
    _bufSize     = bufSize;
    _onDone      = onDone;
    _layoutIdx   = 0;
    _editBtnIdx    = 0xFF;
    _editCharIdx   = 0;
    _lastCommitBtn = 0xFF;
    _focused       = 0;
    _prevFocused = 0xFF;
    _needFullRedraw  = true;
    _needHeaderRedraw = false;
    _needFocusRedraw  = false;
    _prevBtnDown     = false;
    _btnDownMs       = 0;
    _pendingFinish   = false;
    _finishAccepted  = false;
    _active      = true;

    // Disable TcMenu's back button while keyboard is active
    // (we read K0 directly; prevent TcMenu from also processing it)
    switches.replaceOnPressed(PIN_BTN_K0, [](pinid_t, bool) {});

    // Encoder navigates 0..17 (18 button positions)
    switches.changeEncoderPrecision(KB_COUNT - 1, 0);
    renderer.takeOverDisplay(_renderCb);
}

// ---------------------------------------------------------------------------
// _renderCb — called by TcMenu renderer tick and on encoder/button events
// ---------------------------------------------------------------------------

void ScreenKeyboard::_renderCb(unsigned int encoderVal, RenderPressMode pressMode) {
    if (!_active) return;

    // Wait for all buttons to be released before giving back display
    if (_pendingFinish) {
        bool encBtn  = (digitalRead(PIN_ENC_BTN) == LOW);
        bool backBtn = (digitalRead(PIN_BTN_K0)  == LOW);
        if (!encBtn && !backBtn) {
            // Show status message on OK before the callback runs
            if (_finishAccepted) {
                Adafruit_GFX& gfx = menuGetDisplay();
                gfx.fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, COL_BG);
                gfx.setFont(&FreeSansBold9pt7b);
                gfx.setTextSize(1);
                gfx.setTextWrap(false);
                gfx.setTextColor(COL_TEXT);
                gfx.setCursor(50, 120);
                gfx.print("Setting up WiFi...");
                gfx.setFont(nullptr);
            }

            _active = false;
            // Restore TcMenu's back button handler
            switches.replaceOnPressed(PIN_BTN_K0, [](pinid_t, bool held) {
                if (!held) menuMgr.performDirectionMove(true);
            });
            renderer.giveBackDisplay();
            if (_onDone) _onDone(_finishAccepted);
        }
        return;
    }

    // Check T9 timeout
    if (_editBtnIdx != 0xFF && (millis() - _lastEditMs) >= T9_TIMEOUT_MS) {
        _flushEditChar();
        _needHeaderRedraw = true;
    }

    // Encoder position → focused button
    uint8_t focused = static_cast<uint8_t>(encoderVal);
    if (focused >= KB_COUNT) focused = KB_COUNT - 1;

    if (focused != _focused) {
        _lastCommitBtn = 0xFF;   // navigated away — clear re-press block
        _prevFocused = _focused;
        _focused     = focused;
        _needFocusRedraw = true;
    }

    // Handle button press — read GPIO directly because TcMenu's
    // takeOverDisplay sends RPRESS_PRESSED continuously (no edges).
    // Back button (K0) exits the keyboard
    if (digitalRead(PIN_BTN_K0) == LOW) {
        _flushEditChar();
        _finish(false);
        return;
    }

    bool btnDown = (digitalRead(PIN_ENC_BTN) == LOW);   // active-low
    bool risingEdge  = (btnDown && !_prevBtnDown);
    bool fallingEdge = (!btnDown && _prevBtnDown);
    _prevBtnDown = btnDown;

    if (risingEdge) {
        _btnDownMs = millis();
        // Fire action keys immediately on press-down for responsiveness
        const KeyDef& focusKey = _layouts[_layoutIdx][_focused];
        if (focusKey.chars[0] == '\0') {
            _pressButton(_focused);
            return;
        }
    }
    if (fallingEdge) {
        uint32_t held = millis() - _btnDownMs;
        if (held >= 600) {
            // Long press — cancel
            _flushEditChar();
            _finish(false);
            return;
        } else {
            // Short press on character key — fire on release
            const KeyDef& focusKey = _layouts[_layoutIdx][_focused];
            if (focusKey.chars[0] != '\0') {
                _pressButton(_focused);
            }
        }
    }

    // Redraw only what changed
    if (_needFullRedraw) {
        _drawAll();
        _needFullRedraw  = false;
        _needHeaderRedraw = false;
        _needFocusRedraw  = false;
        _prevFocused = 0xFF;
    } else {
        if (_needHeaderRedraw) {
            _drawHeader();
            _needHeaderRedraw = false;
        }
        if (_needFocusRedraw) {
            // Redraw only the two buttons that changed
            if (_prevFocused != 0xFF && _prevFocused < KB_COUNT) {
                _drawButton(_prevFocused, false);
            }
            _drawButton(_focused, true);
            _needFocusRedraw = false;
            _prevFocused = 0xFF;
        }
    }
}

// ---------------------------------------------------------------------------
// _pressButton — handle a key selection
// ---------------------------------------------------------------------------

void ScreenKeyboard::_pressButton(uint8_t idx) {
    const KeyDef& key = _layouts[_layoutIdx][idx];

    // Special / action key
    if (key.chars[0] == '\0') {
        _flushEditChar();

        if (strcmp(key.label, "DEL") == 0) {
            size_t len = strlen(_buf);
            if (len > 0) _buf[len - 1] = '\0';

        } else if (strcmp(key.label, "CLR") == 0) {
            _buf[0] = '\0';

        } else if (strcmp(key.label, "ESC") == 0) {
            _finish(false);
            return;

        } else if (strcmp(key.label, "OK") == 0) {
            _finish(true);
            return;

        } else if (strcmp(key.label, "SPC") == 0) {
            _appendChar(' ');

        } else if (strcmp(key.label, "123") == 0) {
            _setLayout(2);
            return;   // _setLayout already sets _needFullRedraw

        } else if (strcmp(key.label, "ABC") == 0) {
            _setLayout(1);
            return;

        } else if (strcmp(key.label, "abc") == 0) {
            _setLayout(0);
            return;
        }

        _needHeaderRedraw = true;
        return;
    }

    uint8_t numChars = static_cast<uint8_t>(strlen(key.chars));

    // Single-char key: append immediately, no cycling
    if (numChars == 1) {
        _flushEditChar();
        _appendChar(key.chars[0]);
        _needHeaderRedraw = true;
        return;
    }

    // Multi-char T9 cycling
    uint32_t now = millis();
    if (_editBtnIdx == idx && (now - _lastEditMs) < T9_TIMEOUT_MS) {
        // Inside timeout on same button — cycle to next char
        _editCharIdx = (_editCharIdx + 1) % numChars;
    } else if (_lastCommitBtn == idx && _editBtnIdx == 0xFF) {
        // Just committed from this button via timeout — block re-press
        // User must navigate away first
        return;
    } else {
        // New button or timed-out — commit pending and start fresh
        _flushEditChar();
        _editBtnIdx  = idx;
        _editCharIdx = 0;
        _lastCommitBtn = 0xFF;   // clear block
    }
    _lastEditMs = now;
    _needHeaderRedraw = true;
}

// ---------------------------------------------------------------------------
// _appendChar / _flushEditChar
// ---------------------------------------------------------------------------

void ScreenKeyboard::_appendChar(char c) {
    size_t len = strlen(_buf);
    if (len + 1 < _bufSize) {
        _buf[len]     = c;
        _buf[len + 1] = '\0';
    }
}

void ScreenKeyboard::_flushEditChar() {
    if (_editBtnIdx == 0xFF) return;
    const KeyDef& key = _layouts[_layoutIdx][_editBtnIdx];
    if (_editCharIdx < strlen(key.chars)) {
        _appendChar(key.chars[_editCharIdx]);
    }
    _lastCommitBtn = _editBtnIdx;   // block immediate re-press
    _editBtnIdx    = 0xFF;
    _editCharIdx   = 0;
}

// ---------------------------------------------------------------------------
// _setLayout
// ---------------------------------------------------------------------------

void ScreenKeyboard::_setLayout(uint8_t layoutIdx) {
    _flushEditChar();
    _layoutIdx  = layoutIdx;
    _needFullRedraw = true;
}

// ---------------------------------------------------------------------------
// _finish — close the keyboard and return control
// ---------------------------------------------------------------------------

void ScreenKeyboard::_finish(bool accepted) {
    _flushEditChar();
    // Defer giveBackDisplay until all buttons are released,
    // so TcMenu doesn't interpret the still-held button as a menu action.
    _pendingFinish  = true;
    _finishAccepted = accepted;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void ScreenKeyboard::_drawHeader() {
    Adafruit_GFX& gfx = menuGetDisplay();

    gfx.fillRect(0, 0, TFT_WIDTH, KB_HDR_H, COL_BG);

    // Prompt label
    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setTextSize(1);
    gfx.setTextWrap(false);
    gfx.setTextColor(COL_PROMPT);
    gfx.setCursor(4, 17);
    gfx.print(_prompt);

    // Input buffer content
    gfx.setCursor(4, 40);
    gfx.setTextColor(COL_INPUT);
    gfx.print(_buf);

    // T9 pending char in yellow
    if (_editBtnIdx != 0xFF) {
        const KeyDef& key = _layouts[_layoutIdx][_editBtnIdx];
        if (_editCharIdx < strlen(key.chars)) {
            gfx.setTextColor(COL_PEND);
            char tmp[2] = { key.chars[_editCharIdx], '\0' };
            gfx.print(tmp);
        }
    }

    // Cursor bar
    gfx.setTextColor(COL_CURSOR);
    gfx.print("|");

    gfx.setFont(nullptr);
}

void ScreenKeyboard::_drawButton(uint8_t idx, bool focused) {
    Adafruit_GFX& gfx = menuGetDisplay();

    uint8_t col = idx % KB_COLS;
    uint8_t row = idx / KB_COLS;

    int16_t x = col * KB_CELL_W + KB_PAD;
    int16_t y = KB_HDR_H + row * KB_CELL_H + KB_PAD;

    const KeyDef& key = _layouts[_layoutIdx][idx];
    bool isSpecial = (key.chars[0] == '\0');

    // Button fill
    uint16_t bgColor;
    if (focused) {
        bgColor = COL_BTN_FOC;
    } else if (isSpecial) {
        bgColor = COL_BTN_SPL;
    } else {
        bgColor = COL_BTN_NORM;
    }
    gfx.fillRect(x, y, KB_BTN_W, KB_BTN_H, bgColor);

    // Border
    gfx.drawRect(x, y, KB_BTN_W, KB_BTN_H, COL_BORDER);

    // Label — use built-in bitmap font for reliable centering
    gfx.setFont(nullptr);
    gfx.setTextWrap(false);
    gfx.setTextColor(COL_TEXT);

    uint8_t labelLen = static_cast<uint8_t>(strlen(key.label));
    uint8_t tsize    = (labelLen <= 3) ? 2 : 1;  // size 2 for ≤3 chars, 1 for 4-char labels
    gfx.setTextSize(tsize);

    // Pixel size of one character at the chosen text size
    uint8_t charW = 6 * tsize;
    uint8_t charH = 8 * tsize;

    int16_t textW = labelLen * charW;
    int16_t textX = x + (KB_BTN_W - textW) / 2;
    int16_t textY = y + (KB_BTN_H - charH) / 2;

    gfx.setCursor(textX, textY);
    gfx.print(key.label);

    gfx.setTextSize(1);
}

void ScreenKeyboard::_drawAll() {
    Adafruit_GFX& gfx = menuGetDisplay();

    // Clear entire screen once on full redraw
    gfx.fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, COL_BG);

    _drawHeader();

    for (uint8_t i = 0; i < KB_COUNT; i++) {
        _drawButton(i, i == _focused);
    }
}
