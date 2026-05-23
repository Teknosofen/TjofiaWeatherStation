#include "Instruments.h"
#include "config.h"
#include <Arduino.h>

// ── StepperGauge ─────────────────────────────────────────────────────────────

void StepperGauge::setPos(int steps) {
    _pos = steps < 0 ? 0 : (steps > _maxSteps ? _maxSteps : steps);
}

int StepperGauge::valueToSteps(float val) const {
    if (val <= _minVal) return 0;
    if (val >= _maxVal) return _maxSteps;
    return (int)((_maxSteps * (val - _minVal)) / (_maxVal - _minVal));
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

void Instruments::begin(int windSteps, int presSteps) {
    _wind.setPos(windSteps);
    _pres.setPos(presSteps);
}

int Instruments::getWindSteps() const { return _wind.getPos(); }
int Instruments::getPresSteps() const { return _pres.getPos(); }

void Instruments::setWindSpeed(float knots) { _wind.setValue(knots); }
void Instruments::setPressure(float hPa)    { _pres.setValue(hPa);  }

void Instruments::idle() {
    _wind.off();
    _pres.off();
}

void Instruments::stepBoth(int dir, int delayMs) {
    _wind.stepOnce(dir, delayMs);
    _pres.stepOnce(dir, delayMs);
}

void Instruments::selfTest(int steps) {
    for (int i = 0; i < steps; i++) {
        _wind.stepOnce(+1);
        _pres.stepOnce(+1);
    }
    for (int i = 0; i < steps; i++) {
        _wind.stepOnce(-1);
        _pres.stepOnce(-1);
    }
    _wind.off();
    _pres.off();
}
