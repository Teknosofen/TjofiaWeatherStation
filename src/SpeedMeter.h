#pragma once
#include <Arduino.h>

// PWM-driven millivolt meter used as a wind-speed indicator.
// Full-scale = MAX_KNOTS kn; output swings 0 … fullScaleMv (user-calibrated).
class SpeedMeter {
public:
    static constexpr float    MAX_KNOTS     = 30.0f;
    static constexpr float    DEFAULT_FS_MV = 300.0f;

    explicit SpeedMeter(uint8_t pin);

    // Call once in setup(). fullScaleMv = output voltage (mV) at 30 kn.
    void  begin(float fullScaleMv = DEFAULT_FS_MV);

    void  setFullScaleMv(float mv);
    float getFullScaleMv() const { return _fullScaleMv; }
    float getCurrentMv()   const { return _currentMv;   }

    void  setKnots(float knots);
    void  setMillivolts(float mv);

private:
    uint8_t  _pin;
    float    _fullScaleMv;
    float    _currentMv;

    static constexpr float    VCC_MV   = 3300.0f;
    static constexpr int      BITS     = 12;
    static constexpr uint32_t MAX_DUTY = (1u << BITS) - 1;
    static constexpr uint32_t FREQ_HZ  = 1000;

    void writeDuty(float mv);
};
