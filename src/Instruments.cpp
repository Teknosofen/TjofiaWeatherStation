#include "Instruments.h"
#include "config.h"
#include <Arduino.h>

// ── StepperGauge ─────────────────────────────────────────────────────────────

int StepperGauge::valueToSteps(float val) const {
    if (val <= _min) return 0;
    if (val >= _max) return _maxSteps;
    return (int)((_maxSteps * (val - _min)) / (_max - _min));
}

void StepperGauge::setValue(float val, int stepDelay) {
    int target = valueToSteps(val);
    int delta  = target - _pos;
    if (delta == 0) return;
    motor.rotate(abs(delta), delta > 0 ? +1 : -1, stepDelay);
    _pos = target;
    motor.off();
}

// ── Instruments ──────────────────────────────────────────────────────────────

Instruments::Instruments()
    : _wind(MOTOR1_P1, MOTOR1_P2, MOTOR1_P3, MOTOR1_P4,
            WIND_MIN_KT,  WIND_MAX_KT,  WIND_MAX_STEPS),
      _pres(MOTOR2_P1, MOTOR2_P2, MOTOR2_P3, MOTOR2_P4,
            PRES_MIN_HPA, PRES_MAX_HPA, PRES_MAX_STEPS)
{}

void Instruments::begin() {
    // Needles assumed at physical zero on power-up.
    _wind.zero();
    _pres.zero();
}

void Instruments::setWindSpeed(float knots) { _wind.setValue(knots); }
void Instruments::setPressure(float hPa)    { _pres.setValue(hPa);  }

void Instruments::idle() {
    _wind.off();
    _pres.off();
}
