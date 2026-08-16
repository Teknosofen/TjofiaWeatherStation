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

// Absolute move. Leaves the coils energised so a two-leg approach does not let
// the needle settle between legs — the caller is responsible for motor.off().
void StepperGauge::moveTo(int pos, int stepDelay) {
    int delta = pos - _pos;
    if (delta == 0) return;
    motor.rotate(abs(delta), delta > 0 ? +1 : -1, stepDelay);
    _pos = pos;
}

// Drive below the target, then come up onto it.  A geared gauge has lost motion
// between the motor and the needle, so where the needle ends up depends on which
// way it was last driven.  Finishing every move in the increasing direction — the
// direction the calibration wizard is operated in — makes that dependency
// disappear and keeps the stored coefficients valid.
//
// Runs unconditionally, including when the needle is already on target and when
// the target is above it.  Two reasons: nothing outside setValue() can then leave
// the gear train in the wrong state (the startup self-test and the wizard's
// nudge() both end wherever they happen to end), and the dip-and-return is a
// visible sign of life on a barometer whose reading can sit unchanged for hours.
void StepperGauge::approachFromBelow(int target, int stepDelay) {
    moveTo(target - _backlashSteps, stepDelay);
    moveTo(target, stepDelay);              // final leg always increasing
    motor.off();
}

void StepperGauge::setValue(float val, int stepDelay) {
    int target = valueToSteps(val);

    if (_circular) {
        // Shortest arc: reduce both current and target position modulo one full
        // scale, then wrap the difference to [-span/2, +span/2].  _pos itself is
        // never reduced, so the instrument can accumulate multiple rotations.
        const int span = fullScaleSteps();
        int curMod = _pos % span;
        if (curMod < 0) curMod += span;
        int tgtMod = target % span;
        if (tgtMod < 0) tgtMod += span;
        int delta = tgtMod - curMod;
        if (delta >  span / 2) delta -= span;
        if (delta < -span / 2) delta += span;
        target = _pos + delta;
    }

    if (_backlashSteps > 0) {
        approachFromBelow(target, stepDelay);
        return;
    }

    if (target == _pos) return;
    moveTo(target, stepDelay);
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
    // Wind direction: circular dial, pins reversed to match the dial, direct
    // drive so no backlash compensation.
    : _wdir(MOTOR1_P4, MOTOR1_P3, MOTOR1_P2, MOTOR1_P1,
            WDIR_MIN_DEG, WDIR_MAX_DEG, WDIR_MAX_STEPS, true, 0),
      // Pressure: linear dial through a gearbox — approach every target from below.
      _pres(MOTOR2_P1, MOTOR2_P2, MOTOR2_P3, MOTOR2_P4,
            PRES_MIN_HPA, PRES_MAX_HPA, PRES_MAX_STEPS, false, PRES_BACKLASH_STEPS)
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
