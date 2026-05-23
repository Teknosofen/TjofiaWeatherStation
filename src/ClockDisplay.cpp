#include "ClockDisplay.h"
#include <math.h>
#include <Fonts/FreeSans9pt7b.h>

ClockDisplay::ClockDisplay(int8_t cs, int8_t rst)
    : BaseDisplay(cs, rst) {}

// ── Boot-screen overrides — reset clock state before base renders ─────────────

void ClockDisplay::resetClockState() {
    _faceDrawn      = false;
    _lastS          = -1;
    _dispTempBuf[0] = '\0';
    _dispTempX      = -1;
}

void ClockDisplay::showSplash(const String &v, const String &d)
    { resetClockState(); BaseDisplay::showSplash(v, d); }
void ClockDisplay::showStatus(const String &l1, const String &l2)
    { resetClockState(); BaseDisplay::showStatus(l1, l2); }
void ClockDisplay::showError(const String &m)
    { resetClockState(); BaseDisplay::showError(m); }
void ClockDisplay::showAPMode(const String &s)
    { resetClockState(); BaseDisplay::showAPMode(s); }

// ── Private helpers ───────────────────────────────────────────────────────────

void ClockDisplay::drawHand(float angleDeg, int length, int thickness, uint16_t col) {
    float rad = (angleDeg - 90.0f) * (float)M_PI / 180.0f;
    int x2 = CX + (int)(length * cosf(rad));
    int y2 = CY + (int)(length * sinf(rad));
    if (thickness <= 1) {
        _tft.drawLine(CX, CY, x2, y2, col);
    } else {
        for (int d = -(thickness / 2); d <= thickness / 2; d++) {
            float rp = rad + (float)M_PI_2;
            int dx = (int)(d * cosf(rp));
            int dy = (int)(d * sinf(rp));
            _tft.drawLine(CX + dx, CY + dy, x2 + dx, y2 + dy, col);
        }
    }
}

void ClockDisplay::eraseHand(float angleDeg, int length, int thickness) {
    drawHand(angleDeg, length, thickness, COL_BG);
}

void ClockDisplay::drawFace() {
    _tft.fillScreen(COL_BG);
    drawBezel();

    for (int i = 0; i < 60; i++) {
        float rad   = (i * 6 - 90) * (float)M_PI / 180.0f;
        bool isHour = (i % 5 == 0);
        int  r0     = isHour ? R - 10 : R - 5;
        int  x1 = CX + (int)(R  * cosf(rad));
        int  y1 = CY + (int)(R  * sinf(rad));
        int  x2 = CX + (int)(r0 * cosf(rad));
        int  y2 = CY + (int)(r0 * sinf(rad));

        int ox = (int)roundf(-sinf(rad));
        int oy = (int)roundf( cosf(rad));

        _tft.drawLine(x1,      y1,      x2,      y2,      COL_FACE);
        _tft.drawLine(x1 + ox, y1 + oy, x2 + ox, y2 + oy, COL_FACE);
        if (isHour) _tft.drawLine(x1 - ox, y1 - oy, x2 - ox, y2 - oy, COL_FACE);
    }
    _tft.fillCircle(CX, CY, 3, COL_ACCENT);
}

void ClockDisplay::drawCentreText(float tempC) {
    char buf[12];
    if (tempC > -100.0f)
        snprintf(buf, sizeof(buf), "%.1f\xB0" "C", tempC);
    else
        strcpy(buf, "---");

    if (_dispTempBuf[0] != '\0' && strcmp(_dispTempBuf, buf) == 0) return;

    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextSize(1);

    if (_dispTempBuf[0] != '\0') {
        _tft.setTextColor(COL_BG);
        _tft.setCursor(_dispTempX, CY + 40);
        _tft.print(_dispTempBuf);
    }

    int16_t x1, y1; uint16_t tw, th;
    _tft.getTextBounds(buf, 0, 0, &x1, &y1, &tw, &th);
    _dispTempX = CX - (int16_t)(tw / 2);
    _tft.setTextColor(COL_ACCENT);
    _tft.setCursor(_dispTempX, CY + 40);
    _tft.print(buf);

    strncpy(_dispTempBuf, buf, sizeof(_dispTempBuf) - 1);
}

// ── Public API ────────────────────────────────────────────────────────────────

void ClockDisplay::drawClock(int hour, int minute, int second) {
    if (!_faceDrawn) { drawFace(); _faceDrawn = true; }

    float secAngle  = second * 6.0f;
    float minAngle  = minute * 6.0f + second * 0.1f;
    float hourAngle = (hour % 12) * 30.0f + minute * 0.5f;

    auto epX = [](float a, int r) {
        return CX + (int)(r * cosf((a - 90.0f) * (float)M_PI / 180.0f));
    };
    auto epY = [](float a, int r) {
        return CY + (int)(r * sinf((a - 90.0f) * (float)M_PI / 180.0f));
    };

    bool redrawHour = true, redrawMin = true;

    if (_lastS >= 0) {
        float oldSec  = _lastS * 6.0f;
        float oldMin  = _lastM * 6.0f + _lastS * 0.1f;
        float oldHour = (_lastH % 12) * 30.0f + _lastM * 0.5f;

        redrawMin  = (epX(oldMin,  80) != epX(minAngle,  80) ||
                      epY(oldMin,  80) != epY(minAngle,  80));
        redrawHour = (epX(oldHour, 55) != epX(hourAngle, 55) ||
                      epY(oldHour, 55) != epY(hourAngle, 55));

        eraseHand(oldSec,          95, 1);
        eraseHand(oldSec + 180.0f, 20, 1);
        if (redrawMin)  eraseHand(oldMin,  80, 5);
        if (redrawHour) eraseHand(oldHour, 55, 7);
    }

    if (redrawHour) drawHand(hourAngle, 55, 7, COL_FACE);
    if (redrawMin)  drawHand(minAngle,  80, 5, COL_FACE);
    drawHand(secAngle, 95, 1, COL_SEC);

    float tailRad = (secAngle + 90.0f) * (float)M_PI / 180.0f;
    _tft.drawLine(CX, CY,
                  CX + (int)(20 * cosf(tailRad)),
                  CY + (int)(20 * sinf(tailRad)),
                  COL_SEC);

    _tft.fillCircle(CX, CY, 3, COL_ACCENT);

    if (_lastTemp > -999) drawCentreText(_lastTemp);

    _lastH = hour; _lastM = minute; _lastS = second;
}

void ClockDisplay::setTemperature(float tempC) {
    _lastTemp = tempC;
    if (_lastS >= 0) drawCentreText(tempC);
}
