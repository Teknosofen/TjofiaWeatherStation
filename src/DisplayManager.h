#pragma once
#include <Arduino.h>
#include <DIYables_TFT_Round.h>

class DisplayManager {
public:
    DisplayManager();

    bool begin();

    // Analog clock face — call every second in RUNNING state.
    void drawClock(int hour, int minute, int second);

    // Small temperature text drawn in the clock centre.
    void setTemperature(float tempC);

    // Boot splash — branding, version, build date.
    void showSplash(const String &version, const String &buildDate);

    // Full-screen status during boot / transitions.
    void showStatus(const String &line1, const String &line2 = "");

    // Full-screen error — red background.
    void showError(const String &msg);

    // Config-portal notice.
    void showAPMode(const String &ssid);

private:
    DIYables_TFT_GC9A01_Round _tft;

    float   _lastTemp  = -999;
    int     _lastH = -1, _lastM = -1, _lastS = -1;
    bool    _faceDrawn = false;
    char    _dispTempBuf[12] = {};  // text currently painted on screen
    int16_t _dispTempX = -1;        // cursor X of that text

    static constexpr int CX = 120;
    static constexpr int CY = 120;
    static constexpr int R  = 112;

    void drawBezel();
    void drawFace();
    void drawHand(float angleDeg, int length, int thickness, uint16_t colour);
    void eraseHand(float angleDeg, int length, int thickness);
    void drawCentreText(float tempC);
};
