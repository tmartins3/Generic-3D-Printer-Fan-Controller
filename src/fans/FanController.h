#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// FanController.h
// Controls a single 4-pin PWM fan via ESP32 LEDC peripheral.
// Measures fan speed (RPM) by counting tachometer pulses over a fixed window.
//
// 4-pin fan spec:
//   - PWM frequency: 25 kHz (21–28 kHz acceptable)
//   - 100 % duty = maximum speed
//   - Tach output: open-collector, 2 pulses per revolution
//   - Tach needs a pull-up (use ESP32 internal INPUT_PULLUP where possible;
//     GPIO34/35/39 are input-only and do not support pull-ups — use external)
// ---------------------------------------------------------------------------

class FanController {
public:
    // pwmPin     : GPIO connected to fan PWM input (pin 4)
    // tachPin    : GPIO connected to fan tach output (pin 3)
    // ledcChannel: LEDC channel to use (0–15, must be unique per fan)
    FanController(uint8_t pwmPin, uint8_t tachPin, uint8_t ledcChannel);

    // Set up LEDC PWM output and attach the tach interrupt.
    void begin();

    // Mark this fan as present or absent. When not present,
    // setSpeed() is a no-op and getRpm() returns 0.
    void setPresent(bool present) { _present = present; }
    bool isPresent() const { return _present; }

    // Invert PWM signal: 100% duty becomes 0% and vice versa.
    // Useful for NPN+MOSFET driver circuits without logic-level MOSFETs.
    void setInvertPwm(bool invert) { _invertPwm = invert; }
    bool isInvertPwm() const { return _invertPwm; }

    // Set fan speed. percent is clamped to [0, 100].
    // 0 % stops the fan (duty = 0). No-op if fan is not present.
    void setSpeed(uint8_t percent);

    // Change the PWM frequency at runtime (for 2PIN/3PIN fans).
    // Reconfigures the LEDC channel with the new frequency.
    void setFrequency(uint32_t freqHz);

    // Return the last measured speed as a percentage (0–100).
    uint8_t getSpeedPercent() const { return _present ? _speedPercent : 0; }

    // Return the last measured RPM (updated every TACH_MEASURE_MS).
    uint16_t getRpm() const { return _present ? _rpm : 0; }

    // Call this from a periodic TaskManager task every TACH_MEASURE_MS.
    // Computes RPM from the pulse counter and resets it.
    void updateRpm();

    // ISR-safe pulse counter incremented by the tach interrupt.
    // Public so the static ISR wrapper can access it.
    volatile uint32_t _pulseCount;

private:
    uint8_t  _pwmPin;
    uint8_t  _tachPin;
    uint8_t  _ledcChannel;
    uint8_t  _speedPercent;
    uint16_t _rpm;
    bool     _present;
    bool     _invertPwm;
};
