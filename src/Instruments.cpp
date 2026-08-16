#include "Instruments.h"
#include "config.h"
#include <Arduino.h>

// ── StepperGauge ─────────────────────────────────────────────────────────────

void StepperGauge::setPos(int steps) {
    _pos = steps;
}

// Steps for one complete pass over the value range, derived from the *calibrated*
// gain rather than _maxSteps — a re-calibrated gauge may need more than one
// revolution of the output shaft to cover its scale.
int StepperGauge::fullScaleSteps() const {
    int span = (int)lroundf(fabsf(_stepsPerUnit) * (_maxVal - _minVal));
    return span > 0 ? span : _maxSteps;
}

int StepperGauge::valueToSteps(float val) const {
    // Deliberately unclamped: _maxSteps only seeds the factory-default gain.
    // The needle may sit past nominal full scale, and circular gauges accumulate
    // whole revolutions, so capping the target here would freeze the needle.
    return (int)lroundf(_zeroSteps + _stepsPerUnit * (val - _minVal));
}

void StepperGauge::setValue(float val, int stepDelay) {
    int target = valueToSteps(val);
    int delta;
    if (_circular) {
        // Shortest arc: reduce both current and target position modulo one full
        // scale, then wrap the difference to [-span/2, +span/2].  _pos itself is
        // never reduced, so the instrument can accumulate multiple rotations.
        const int span = fullScaleSteps();
        int curMod = _pos % span;
        if (curMod < 0) curMod += span;
        int tgtMod = target % span;
        if (tgtMod < 0) tgtMod += span;
        delta = tgtMod - curMod;
        if (delta >  span / 2) delta -= span;
        if (delta < -span / 2) delta += span;
    } else {
        delta = target - _pos;
    }
    if (delta == 0) return;
    motor.rotate(abs(delta), delta > 0 ? +1 : -1, stepDelay);
    _pos += delta;
    motor.off();
}

void StepperGauge::nudge(int delta, int stepDelay) {
    if (delta == 0) return;
    // No end stop: the calibration wizard must be able to drive the needle
    // anywhere on the dial, including past _maxSteps and over several revolutions.
    motor.rotate(abs(delta), delta > 0 ? +1 : -1, stepDelay);
    _pos += delta;
    motor.off();
}

void StepperGauge::setCalibration(float zeroSteps, float stepsPerUnit) {
    _zeroSteps    = zeroSteps;
    _stepsPerUnit = stepsPerUnit;
    _calibrated   = true;
}

void StepperGauge::getCalibration(float &zeroSteps, float &stepsPerUnit) const {
    zeroSteps    = _zeroSteps;
    stepsPerUnit = _stepsPerUnit;
}

void StepperGauge::resetCalibration() {
    _zeroSteps    = 0.0f;
    _stepsPerUnit = (float)_maxSteps / (_maxVal - _minVal);
    _calibrated   = false;
}

// ── Instruments ──────────────────────────────────────────────────────────────

Instruments::Instruments()
    : _wdir(MOTOR1_P4, MOTOR1_P3, MOTOR1_P2, MOTOR1_P1,
            WDIR_MIN_DEG, WDIR_MAX_DEG, WDIR_MAX_STEPS, true),   // circular; pins reversed to match dial
      _pres(MOTOR2_P1, MOTOR2_P2, MOTOR2_P3, MOTOR2_P4,
            PRES_MIN_HPA, PRES_MAX_HPA, PRES_MAX_STEPS)
{}

void Instruments::begin(int wdirSteps, int presSteps) {
    _wdir.setPos(wdirSteps);
    _pres.setPos(presSteps);
}

int Instruments::getWdirSteps() const { return _wdir.getPos(); }
int Instruments::getPresSteps() const { return _pres.getPos(); }

void Instruments::setWindDir(float deg) { _wdir.setValue(deg); }
void Instruments::setPressure(float hPa)    { _pres.setValue(hPa);  }

void Instruments::idle() {
    _wdir.off();
    _pres.off();
}

void Instruments::stepBoth(int dir, int delayMs) {
    _wdir.stepOnce(dir, delayMs);
    _pres.stepOnce(dir, delayMs);
}

void Instruments::selfTest(int steps) {
    for (int i = 0; i < steps; i++) {
        _wdir.stepOnce(+1);
        _pres.stepOnce(+1);
    }
    for (int i = 0; i < steps; i++) {
        _wdir.stepOnce(-1);
        _pres.stepOnce(-1);
    }
    _wdir.off();
    _pres.off();
}
