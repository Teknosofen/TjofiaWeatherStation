#include "GaugeCalWizard.h"
#include "config.h"

int GaugeCalWizard::position() const {
    return _isWdir ? _inst.getWdirSteps() : _inst.getPresSteps();
}

float GaugeCalWizard::minValue() const {
    return _isWdir ? (float)WDIR_MIN_DEG : (float)PRES_MIN_HPA;
}

float GaugeCalWizard::maxValue() const {
    return _isWdir ? (float)WDIR_MAX_DEG : (float)PRES_MAX_HPA;
}

void GaugeCalWizard::start(bool wdir) {
    _isWdir = wdir;
    _step   = Step::P1;
}

int GaugeCalWizard::nudge(int delta) {
    // Coarse moves run faster; fine moves keep full torque margin.
    int stepDelay = (abs(delta) > 100) ? 3 : 5;
    if (_isWdir) _inst.nudgeWdir(delta, stepDelay);
    else         _inst.nudgePres(delta, stepDelay);
    return position();
}

bool GaugeCalWizard::confirm(float val) {
    if (_step == Step::IDLE) return false;

    if (_step == Step::P1) {
        _p1Val   = val;
        _p1Steps = position();
        _step    = Step::P2;
        return false;
    }

    computeAndStore(val, position());
    _step = Step::IDLE;
    return true;
}

void GaugeCalWizard::computeAndStore(float p2Val, int p2Steps) {
    float dv = p2Val - _p1Val;
    int   ds = p2Steps - _p1Steps;
    if (fabsf(dv) <= 0.1f || ds == 0) {
        Serial.println("Gauge cal: P1==P2 or identical steps, discarded");
        return;
    }

    float gain      = (float)ds / dv;
    float zeroSteps = (float)_p1Steps - gain * (_p1Val - minValue());
    if (gain <= 0.0f) {
        Serial.println("Gauge cal: computed gain <= 0, discarded");
        return;
    }

    if (_isWdir) _inst.setWdirCalibration(zeroSteps, gain);
    else         _inst.setPresCalibration(zeroSteps, gain);
    _cfg.saveGaugeCal(_isWdir, zeroSteps, gain);
    Serial.printf("%s cal saved: zero=%.2f  gain=%.4f steps/%s\n",
                  _isWdir ? "Wdir" : "Pres", zeroSteps, gain,
                  _isWdir ? "deg" : "hPa");
}

void GaugeCalWizard::resetGauge(bool wdir) {
    if (wdir) _inst.resetWdirCalibration();
    else      _inst.resetPresCalibration();
    _cfg.clearGaugeCal(wdir);
    Serial.printf("%s calibration reset to factory default\n", wdir ? "Wdir" : "Pres");
}
