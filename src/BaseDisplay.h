#pragma once
#include <Arduino.h>
#include <DIYables_TFT_Round.h>
#include "config.h"

class BaseDisplay {
public:
    // rst=-1  → skip hardware reset pulse (use for secondary displays
    //           sharing the RST line with an already-initialised primary).
    BaseDisplay(int8_t cs, int8_t rst = TFT_RST);

    bool begin();

    void showSplash(const String &version, const String &buildDate);
    void showStatus(const String &line1, const String &line2 = "");
    void showError(const String &msg);
    void showAPMode(const String &ssid);

protected:
    DIYables_TFT_GC9A01_Round _tft;

    static constexpr int CX = 120;
    static constexpr int CY = 120;
    static constexpr int R  = 112;

    void drawBezel();
};
