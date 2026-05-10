#include "DisplayManager.h"
#include "config.h"
#include <math.h>

#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

DisplayManager::DisplayManager()
    : _tft(TFT_RST, TFT_DC, TFT_CS) {}

bool DisplayManager::begin() {
    _tft.begin();

    if (TFT_BL >= 0) {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }

    _tft.fillScreen(COL_BG);
    return true;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void DisplayManager::drawHand(float angleDeg, int length, int thickness, uint16_t col) {
    float rad = (angleDeg - 90.0f) * (float)M_PI / 180.0f;
    int x2 = CX + (int)(length * cosf(rad));
    int y2 = CY + (int)(length * sinf(rad));
    if (thickness <= 1) {
        _tft.drawLine(CX, CY, x2, y2, col);
    } else {
        for (int d = -(thickness/2); d <= thickness/2; d++) {
            float rp = rad + (float)M_PI_2;
            int dx = (int)(d * cosf(rp));
            int dy = (int)(d * sinf(rp));
            _tft.drawLine(CX + dx, CY + dy, x2 + dx, y2 + dy, col);
        }
    }
}

void DisplayManager::eraseHand(float angleDeg, int length, int thickness) {
    drawHand(angleDeg, length, thickness, COL_BG);
}

void DisplayManager::drawFace() {
    _tft.drawCircle(CX, CY, R,     COL_FACE);
    _tft.drawCircle(CX, CY, R - 1, COL_FACE);

    for (int i = 0; i < 60; i++) {
        float rad = (i * 6 - 90) * (float)M_PI / 180.0f;
        int isHour = (i % 5 == 0);
        int r0 = isHour ? R - 10 : R - 5;
        int x1 = CX + (int)(R  * cosf(rad));
        int y1 = CY + (int)(R  * sinf(rad));
        int x2 = CX + (int)(r0 * cosf(rad));
        int y2 = CY + (int)(r0 * sinf(rad));
        _tft.drawLine(x1, y1, x2, y2, isHour ? COL_FACE : (uint16_t)0x7BEF);
    }
    _tft.fillCircle(CX, CY, 3, COL_ACCENT);
}

void DisplayManager::drawCentreText(float tempC) {
    _tft.fillRect(CX - 45, CY + 20, 90, 28, COL_BG);
    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_ACCENT);
    _tft.setTextSize(1);

    char buf[12];
    if (tempC > -100.0f) {
        snprintf(buf, sizeof(buf), "%.1f\xB0" "C", tempC);
    } else {
        strcpy(buf, "---");
    }
    int16_t x1, y1; uint16_t tw, th;
    _tft.getTextBounds(buf, 0, 0, &x1, &y1, &tw, &th);
    _tft.setCursor(CX - tw / 2, CY + 40);
    _tft.print(buf);
}

// ── Public API ────────────────────────────────────────────────────────────────

void DisplayManager::showSplash(const String &version, const String &buildDate) {
    _tft.fillScreen(COL_BG);
    _tft.drawCircle(CX, CY, R,     COL_FACE);
    _tft.drawCircle(CX, CY, R - 1, COL_FACE);

    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextSize(1);

    auto centre = [&](const char *s, int y, uint16_t col) {
        int16_t x1, y1; uint16_t tw, th;
        _tft.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
        _tft.setTextColor(col);
        _tft.setCursor(CX - tw / 2, y);
        _tft.print(s);
    };

    centre("Teknosofen", 110, COL_FACE);
    centre(version.c_str(),  150, COL_FACE);
    centre(buildDate.c_str(), 170, 0x7BEF);       // dim white for build date
}

void DisplayManager::drawClock(int hour, int minute, int second) {
    static bool faceDrawn = false;
    if (!faceDrawn) { drawFace(); faceDrawn = true; }

    if (_lastS >= 0) {
        eraseHand(_lastS * 6.0f,                          95, 1);
        eraseHand(_lastM * 6.0f + _lastS * 0.1f,          80, 3);
        eraseHand((_lastH % 12) * 30.0f + _lastM * 0.5f,  55, 5);
        drawFace();
    }

    float secAngle  = second * 6.0f;
    float minAngle  = minute * 6.0f + second * 0.1f;
    float hourAngle = (hour % 12) * 30.0f + minute * 0.5f;

    drawHand(hourAngle, 55, 5, COL_FACE);
    drawHand(minAngle,  80, 3, COL_FACE);
    drawHand(secAngle,  95, 1, COL_SEC);

    float tailRad = (secAngle - 90.0f + 180.0f) * (float)M_PI / 180.0f;
    _tft.drawLine(CX, CY,
                  CX + (int)(20 * cosf(tailRad)),
                  CY + (int)(20 * sinf(tailRad)),
                  COL_SEC);

    _tft.fillCircle(CX, CY, 3, COL_ACCENT);

    if (_lastTemp > -999) drawCentreText(_lastTemp);

    _lastH = hour; _lastM = minute; _lastS = second;
}

void DisplayManager::setTemperature(float tempC) {
    _lastTemp = tempC;
    if (_lastS >= 0) drawCentreText(tempC);
}

void DisplayManager::showStatus(const String &line1, const String &line2) {
    _tft.fillScreen(COL_BG);
    _tft.drawCircle(CX, CY, R,     COL_FACE);
    _tft.drawCircle(CX, CY, R - 1, COL_FACE);

    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_FACE);
    _tft.setTextSize(1);

    int16_t x1, y1; uint16_t tw, th;
    _tft.getTextBounds(line1.c_str(), 0, 0, &x1, &y1, &tw, &th);
    _tft.setCursor(CX - tw / 2, line2.isEmpty() ? CY + 6 : CY - 4);
    _tft.print(line1);

    if (!line2.isEmpty()) {
        _tft.getTextBounds(line2.c_str(), 0, 0, &x1, &y1, &tw, &th);
        _tft.setCursor(CX - tw / 2, CY + 20);
        _tft.setTextColor(COL_ACCENT);
        _tft.print(line2);
    }
    _lastS = -1;
}

void DisplayManager::showError(const String &msg) {
    _tft.fillScreen(COL_BG);
    _tft.drawCircle(CX, CY, R, COL_ERROR);

    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_ERROR);
    _tft.setTextSize(1);
    _tft.setCursor(CX - 22, CY - 10);
    _tft.print("ERROR");

    _tft.setTextColor(COL_FACE);
    _tft.setFont(nullptr);
    _tft.setTextSize(1);
    _tft.setCursor(8, CY + 15);
    _tft.setTextWrap(true);
    _tft.print(msg);
    _lastS = -1;
}

void DisplayManager::showAPMode(const String &ssid) {
    _tft.fillScreen(COL_BG);
    _tft.drawCircle(CX, CY, R, 0x07FF);

    _tft.setFont(&FreeSans9pt7b);

    auto centreText = [&](const String &s, int y, uint16_t col) {
        int16_t x1, y1; uint16_t tw, th;
        _tft.getTextBounds(s.c_str(), 0, 0, &x1, &y1, &tw, &th);
        _tft.setTextColor(col);
        _tft.setCursor(CX - tw / 2, y);
        _tft.print(s);
    };

    centreText("WiFi Setup",  CY - 30, 0x07FF);
    centreText("Connect to:", CY -  8, COL_FACE);
    centreText(ssid,          CY + 14, COL_ACCENT);
    centreText("192.168.4.1", CY + 36, 0x7BEF);
    _lastS = -1;
}
