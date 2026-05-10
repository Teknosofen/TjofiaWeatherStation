#pragma once
#include "Stepper28BYJ.h"

// Position-aware stepper gauge: tracks current position and moves only the delta.
class StepperGauge {
public:
    StepperGauge(int p1, int p2, int p3, int p4, float minVal, float maxVal, int maxSteps)
        : motor(p1, p2, p3, p4),
          _minVal(minVal), _maxVal(maxVal), _maxSteps(maxSteps),
          _pos(0) {}

    // Set assumed position without moving the motor.
    // Use on startup to restore the last persisted step count from NVS.
    void setPos(int steps);
    int  getPos() const { return _pos; }

    // Move needle to represent `val` (clamped to [min, max]).
    void setValue(float val, int stepDelay = 3);

    void off() { motor.off(); }

private:
    Stepper28BYJ motor;
    float _minVal, _maxVal;
    int   _maxSteps;
    int   _pos;

    int valueToSteps(float val) const;
};

// Thin wrapper around both gauges.
class Instruments {
public:
    Instruments();

    // windSteps / presSteps: step positions restored from NVS (0 = needles at minimum).
    void begin(int windSteps = 0, int presSteps = 0);
    void setWindSpeed(float knots);   // Motor 1
    void setPressure(float hPa);      // Motor 2
    void idle();                       // de-energise coils to save power
    int  getWindSteps() const;
    int  getPresSteps() const;

private:
    StepperGauge _wind;
    StepperGauge _pres;
};
