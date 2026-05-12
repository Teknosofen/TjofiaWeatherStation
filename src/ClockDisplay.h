#pragma once
#include "BaseDisplay.h"

class ClockDisplay : public BaseDisplay {
public:
    explicit ClockDisplay(int8_t cs, int8_t rst = TFT_RST);

    void drawClock(int hour, int minute, int second);
    void setTemperature(float tempC);

    // Overrides reset internal clock state before delegating to BaseDisplay.
    void showSplash(const String &version, const String &buildDate);
    void showStatus(const String &line1, const String &line2 = "");
    void showError(const String &msg);
    void showAPMode(const String &ssid);

private:
    float   _lastTemp  = -999;
    int     _lastH = -1, _lastM = -1, _lastS = -1;
    bool    _faceDrawn = false;
    char    _dispTempBuf[12] = {};
    int16_t _dispTempX = -1;

    void resetClockState();
    void drawFace();
    void drawHand(float angleDeg, int length, int thickness, uint16_t col);
    void eraseHand(float angleDeg, int length, int thickness);
    void drawCentreText(float tempC);
};
