#pragma once
#include "Stepper28BYJ.h"

// Position-aware stepper gauge with two-point linear calibration.
class StepperGauge {
public:
    StepperGauge(int p1, int p2, int p3, int p4, float minVal, float maxVal, int maxSteps,
                 bool circular = false)
        : motor(p1, p2, p3, p4),
          _minVal(minVal), _maxVal(maxVal), _maxSteps(maxSteps),
          _pos(0),
          _zeroSteps(0.0f),
          _stepsPerUnit((float)maxSteps / (maxVal - minVal)),
          _calibrated(false),
          _circular(circular) {}

    // Set assumed position without moving the motor (NVS restore on boot).
    void setPos(int steps);
    int  getPos() const { return _pos; }

    // Move needle to represent val.  The step position is not clamped — a
    // circular gauge takes the shortest arc and may accumulate whole revolutions.
    void setValue(float val, int stepDelay = 5);

    // Move the motor by delta steps and update the tracked position.
    // Used during the calibration wizard.
    void nudge(int delta, int stepDelay = 5);

    // Two-point calibration coefficients.
    void setCalibration(float zeroSteps, float stepsPerUnit);
    void getCalibration(float &zeroSteps, float &stepsPerUnit) const;
    void resetCalibration();           // revert to factory-default coefficients
    bool isCalibrated() const { return _calibrated; }

    void off() { motor.off(); }

    // Advance one half-step without updating _pos (startup self-test only).
    void stepOnce(int dir, int delayMs = 3) { motor.step(dir, delayMs); }

private:
    Stepper28BYJ motor;
    float _minVal, _maxVal;
    int   _maxSteps;      // nominal full-scale travel; seeds the factory gain only
    int   _pos;           // free-running; never clamped or wrapped
    float _zeroSteps;     // steps at _minVal (calibration offset)
    float _stepsPerUnit;  // steps per physical unit (calibration gain)
    bool  _calibrated;
    bool  _circular;      // true → shortest-arc movement, _pos accumulates freely

    int valueToSteps(float val) const;
    int fullScaleSteps() const;   // calibrated steps across the whole value range
};

// Thin wrapper around both gauges.
class Instruments {
public:
    Instruments();

    // wdirSteps / presSteps: step positions restored from NVS (0 = needles at minimum).
    void begin(int wdirSteps = 0, int presSteps = 0);
    void setWindDir(float deg);
    void setPressure(float hPa);
    void idle();
    int  getWdirSteps() const;
    int  getPresSteps() const;

    // Calibration wizard support — nudge one gauge, read/write calibration.
    void nudgeWdir(int delta, int stepDelay = 5)              { _wdir.nudge(delta, stepDelay); }
    void nudgePres(int delta, int stepDelay = 5)             { _pres.nudge(delta, stepDelay); }
    void setWdirCalibration(float zero, float gain)           { _wdir.setCalibration(zero, gain); }
    void setPresCalibration(float zero, float gain)           { _pres.setCalibration(zero, gain); }
    void getWdirCalibration(float &zero, float &gain) const   { _wdir.getCalibration(zero, gain); }
    void getPresCalibration(float &zero, float &gain) const   { _pres.getCalibration(zero, gain); }
    bool wdirCalibrated() const                               { return _wdir.isCalibrated(); }
    bool presCalibrated() const                               { return _pres.isCalibrated(); }
    void resetWdirCalibration()                               { _wdir.resetCalibration(); }
    void resetPresCalibration()                               { _pres.resetCalibration(); }

    // Step both motors one half-step simultaneously (startup self-test).
    void stepBoth(int dir, int delayMs = 3);
    void selfTest(int steps = 341);

private:
    StepperGauge _wdir;
    StepperGauge _pres;
};
