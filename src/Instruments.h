#pragma once
#include "Stepper28BYJ.h"

// Position-aware stepper gauge: tracks current position and moves only the delta.
class StepperGauge {
public:
    StepperGauge(int p1, int p2, int p3, int p4, float minVal, float maxVal, int maxSteps)
        : motor(p1, p2, p3, p4),
          _min(minVal), _max(maxVal), _maxSteps(maxSteps),
          _pos(0) {}

    // Drive to zero position (call once at startup before any setValue).
    // The gauge needle must be at its physical minimum stop when power-on.
    void zero() { _pos = 0; }

    // Move needle to represent `val` (clamped to [min, max]).
    void setValue(float val, int stepDelay = 3);

    void off() { motor.off(); }

private:
    Stepper28BYJ motor;
    float _min, _max;
    int   _maxSteps;
    int   _pos;

    int valueToSteps(float val) const;
};

// Thin wrapper around both gauges.
class Instruments {
public:
    Instruments();

    void begin();
    void setWindSpeed(float knots);   // Motor 1
    void setPressure(float hPa);      // Motor 2
    void idle();                       // de-energise coils to save power

private:
    StepperGauge _wind;
    StepperGauge _pres;
};
