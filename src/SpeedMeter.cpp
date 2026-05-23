#include "SpeedMeter.h"

SpeedMeter::SpeedMeter(uint8_t pin)
    : _pin(pin), _fullScaleMv(DEFAULT_FS_MV), _currentMv(0.0f) {}

void SpeedMeter::begin(float fullScaleMv) {
    setFullScaleMv(fullScaleMv);
    ledcAttach(_pin, FREQ_HZ, BITS);
    writeDuty(0.0f);
}

void SpeedMeter::setFullScaleMv(float mv) {
    if (mv <    1.0f) mv =    1.0f;
    if (mv > VCC_MV)  mv = VCC_MV;
    _fullScaleMv = mv;
}

void SpeedMeter::setKnots(float knots) {
    if (knots <       0.0f) knots =       0.0f;
    if (knots > MAX_KNOTS)  knots = MAX_KNOTS;
    setMillivolts(knots / MAX_KNOTS * _fullScaleMv);
}

void SpeedMeter::setMillivolts(float mv) {
    if (mv <    0.0f) mv =    0.0f;
    if (mv > VCC_MV)  mv = VCC_MV;
    writeDuty(mv);
}

void SpeedMeter::writeDuty(float mv) {
    _currentMv       = mv;
    uint32_t duty    = (uint32_t)(mv / VCC_MV * (float)MAX_DUTY + 0.5f);
    if (duty > MAX_DUTY) duty = MAX_DUTY;
    ledcWrite(_pin, duty);
}
