#include "TemperatureSensors.h"
#include "../../include/Config.h"

// ---------------------------------------------------------------------------
// TemperatureSensors.cpp
// ---------------------------------------------------------------------------

TemperatureSensors::TemperatureSensors()
    : _chamberBus(PIN_TEMP_CHAMBER),
      _bedBus(PIN_TEMP_BED),
      _chamberSensor(&_chamberBus),
      _bedSensor(&_bedBus),
      _chamberTemp(TEMP_READ_ERROR),
      _bedTemp(TEMP_READ_ERROR),
      _chamberOk(false),
      _bedOk(false)
{}

void TemperatureSensors::begin() {
    _chamberSensor.begin();
    _bedSensor.begin();

    // Set 12-bit resolution (~750 ms conversion time).
    _chamberSensor.setResolution(TEMP_RESOLUTION_BITS);
    _bedSensor.setResolution(TEMP_RESOLUTION_BITS);

    // Non-blocking mode: requestTemperatures() returns immediately and we
    // read the result after the conversion window has elapsed.
    _chamberSensor.setWaitForConversion(false);
    _bedSensor.setWaitForConversion(false);

    Serial.printf("[Temp] Chamber bus: %d device(s)\n",
                  _chamberSensor.getDeviceCount());
    Serial.printf("[Temp] Bed bus:     %d device(s)\n",
                  _bedSensor.getDeviceCount());
}

void TemperatureSensors::requestAll() {
    _chamberSensor.requestTemperatures();
    _bedSensor.requestTemperatures();
}

void TemperatureSensors::update() {
    // getTempCByIndex returns DEVICE_DISCONNECTED_C (-127.0) on failure,
    // which matches our TEMP_READ_ERROR sentinel.
    _chamberTemp = _chamberSensor.getTempCByIndex(0);
    _bedTemp     = _bedSensor.getTempCByIndex(0);

    _chamberOk = (_chamberTemp > TEMP_READ_ERROR + 1.0f);
    _bedOk     = (_bedTemp     > TEMP_READ_ERROR + 1.0f);

    if (!_chamberOk) {
        Serial.println("[Temp] WARNING: Chamber sensor read failed");
    }
    if (!_bedOk) {
        Serial.println("[Temp] WARNING: Bed sensor read failed");
    }
}
