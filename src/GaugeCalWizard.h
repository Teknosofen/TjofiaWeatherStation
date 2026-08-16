#pragma once
#include <Arduino.h>
#include "Instruments.h"
#include "Settings.h"

// Two-point linear calibration wizard for one stepper gauge at a time.
//
// The operator nudges the needle to a printed mark, types the value it points
// at, and repeats for a second mark. From the two (steps, value) pairs the
// wizard derives gain (steps per unit) and zero (steps at the scale minimum)
// and persists them. While the wizard is active the caller must leave the
// gauges alone — active() says when that is the case.
class GaugeCalWizard {
public:
    enum class Step { IDLE, P1, P2 };

    GaugeCalWizard(Instruments &instruments, Settings &settings)
        : _inst(instruments), _cfg(settings) {}

    bool  active()      const { return _step != Step::IDLE; }
    Step  step()        const { return _step; }
    bool  isWdir()      const { return _isWdir; }
    int   pointNumber() const { return _step == Step::P2 ? 2 : 1; }
    float p1Value()     const { return _p1Val; }
    int   p1Steps()     const { return _p1Steps; }

    // Live step count of the gauge being calibrated.
    int position() const;

    // Scale bounds of the gauge being calibrated (for the input field limits).
    float minValue() const;
    float maxValue() const;

    void start(bool wdir);
    void cancel() { _step = Step::IDLE; }

    // Drive the needle; returns the new step position.
    int nudge(int delta);

    // Record the value the needle points at. Returns true when the second
    // point completed the wizard (calibration computed, stored, wizard idle).
    bool confirm(float val);

    // Discard a stored calibration and revert the gauge to its factory gain.
    void resetGauge(bool wdir);

private:
    Instruments &_inst;
    Settings    &_cfg;

    Step  _step    = Step::IDLE;
    bool  _isWdir  = true;
    float _p1Val   = 0.0f;
    int   _p1Steps = 0;

    void computeAndStore(float p2Val, int p2Steps);
};
