#pragma once
#include "BaseDisplay.h"

class ClockDisplay : public BaseDisplay {
public:
    explicit ClockDisplay(int8_t cs, int8_t rst = TFT_RST);

    void drawClock(int hour, int minute, int second);

    // Overrides reset internal clock state before delegating to BaseDisplay.
    void showSplash(const String &version, const String &buildDate);
    void showStatus(const String &line1, const String &line2 = "");
    void showError(const String &msg);
    void showAPMode(const String &ssid);

private:
    int  _lastH = -1, _lastM = -1, _lastS = -1;
    char _dateBuf[12] = {};   // last rendered date string, used for erase pass
    bool _faceDrawn = false;

    void resetClockState();
    void drawFace();
    void drawHand(float angleDeg, int length, int thickness, uint16_t col);
    void eraseHand(float angleDeg, int length, int thickness);
};
