#include "FanController.h"
#include "../../include/Config.h"

// ---------------------------------------------------------------------------
// FanController.cpp
// ---------------------------------------------------------------------------

// Static ISR wrappers — one per fan instance. We support up to 3 fans.
// Each wrapper increments the pulse counter of the associated FanController.
// The instances are registered in begin() via attachInterrupt().

static FanController* _fanInstances[3] = { nullptr, nullptr, nullptr };

static void IRAM_ATTR tachISR0() { if (_fanInstances[0]) _fanInstances[0]->_pulseCount++; }
static void IRAM_ATTR tachISR1() { if (_fanInstances[1]) _fanInstances[1]->_pulseCount++; }
static void IRAM_ATTR tachISR2() { if (_fanInstances[2]) _fanInstances[2]->_pulseCount++; }

static void (*_tachISRs[3])() = { tachISR0, tachISR1, tachISR2 };

// Counter to assign ISR slots sequentially.
static uint8_t _nextIsrSlot = 0;

// ---------------------------------------------------------------------------

FanController::FanController(uint8_t pwmPin, uint8_t tachPin, uint8_t ledcChannel)
    : _pwmPin(pwmPin),
      _tachPin(tachPin),
      _ledcChannel(ledcChannel),
      _speedPercent(0),
      _rpm(0),
      _pulseCount(0),
      _present(true),
      _invertPwm(false)
{}

void FanController::begin() {
    // Configure LEDC PWM output.
    ledcSetup(_ledcChannel, FAN_PWM_FREQ_HZ, FAN_PWM_RESOLUTION);
    ledcAttachPin(_pwmPin, _ledcChannel);
    ledcWrite(_ledcChannel, 0);  // Start with fan off

    // Configure tach input. GPIO34/35/39 are input-only and do not support
    // internal pull-ups — external resistors are required on those pins.
    // For pins that support it, INPUT_PULLUP is used as a safety fallback,
    // but the schematic should always provide an external pull-up on tach lines.
    pinMode(_tachPin, INPUT_PULLUP);

    // Attach rising-edge interrupt for pulse counting.
    if (_nextIsrSlot < 3) {
        _fanInstances[_nextIsrSlot] = this;
        attachInterrupt(digitalPinToInterrupt(_tachPin),
                        _tachISRs[_nextIsrSlot],
                        FALLING);  // Tach pulses are active-low open-collector
        _nextIsrSlot++;
    } else {
        Serial.println("[Fan] ERROR: No ISR slot available for tach interrupt");
    }

    Serial.printf("[Fan] Init PWM pin %d (LEDC ch %d), tach pin %d\n",
                  _pwmPin, _ledcChannel, _tachPin);
}

void FanController::setFrequency(uint32_t freqHz) {
    ledcSetup(_ledcChannel, freqHz, FAN_PWM_RESOLUTION);
    // Re-apply current speed at new frequency
    uint32_t duty = map(_speedPercent, 0, 100, 0, FAN_PWM_MAX_DUTY);
    if (_invertPwm) duty = FAN_PWM_MAX_DUTY - duty;
    ledcWrite(_ledcChannel, duty);
    Serial.printf("[Fan] Pin %d freq set to %lu Hz\n", _pwmPin, freqHz);
}

void FanController::setSpeed(uint8_t percent) {
    if (!_present) { _speedPercent = 0; return; }
    _speedPercent = constrain(percent, 0, 100);
    uint32_t duty = map(_speedPercent, 0, 100, 0, FAN_PWM_MAX_DUTY);
    if (_invertPwm) duty = FAN_PWM_MAX_DUTY - duty;
    ledcWrite(_ledcChannel, duty);
}

void FanController::updateRpm() {
    // Snapshot and reset the pulse counter atomically.
    noInterrupts();
    uint32_t pulses = _pulseCount;
    _pulseCount = 0;
    interrupts();

    // 2 pulses per revolution; measurement window is TACH_MEASURE_MS.
    // RPM = (pulses / 2) / (TACH_MEASURE_MS / 60000)
    //     = pulses * 30000 / TACH_MEASURE_MS
    _rpm = static_cast<uint16_t>(pulses * 30000UL / TACH_MEASURE_MS);
}
