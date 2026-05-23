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

    // Advance one half-step without updating _pos.
    // Use only when an equal and opposite step will follow (net zero displacement).
    void stepOnce(int dir, int delayMs = 3) { motor.step(dir, delayMs); }

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

    // Step both motors one half-step in the same direction simultaneously.
    // Use for custom sweep sequences (e.g. startup self-test with PWM interleaved).
    // Does not update internal position tracking — caller must ensure net-zero movement.
    void stepBoth(int dir, int delayMs = 3);

    // Startup self-test: sweep both needles +steps then −steps simultaneously.
    // Net displacement is zero so restored NVS positions remain accurate.
    // steps=341 ≈ 30° of output-shaft rotation (4096 steps/rev).
    void selfTest(int steps = 341);

private:
    StepperGauge _wind;
    StepperGauge _pres;
};
