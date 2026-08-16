#include "ClockDisplay.h"
#include <math.h>
#include <time.h>
#include <Fonts/FreeSans9pt7b.h>

ClockDisplay::ClockDisplay(int8_t cs, int8_t rst)
    : BaseDisplay(cs, rst) {}

// ── Boot-screen overrides — reset clock state before base renders ─────────────

void ClockDisplay::resetClockState() {
    _faceDrawn = false;
    _lastS     = -1;
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
        float rp   = rad + (float)M_PI_2;
        float half = thickness / 2.0f;
        int   dx   = (int)(half * cosf(rp));
        int   dy   = (int)(half * sinf(rp));
        // Draw hand as a filled parallelogram (two triangles).
        // fillTriangle uses drawFastHLine scan-lines — one SPI burst per row.
        _tft.fillTriangle(CX + dx, CY + dy, CX - dx, CY - dy, x2 + dx, y2 + dy, col);
        _tft.fillTriangle(CX - dx, CY - dy, x2 + dx, y2 + dy, x2 - dx, y2 - dy, col);
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

// ── Public API ────────────────────────────────────────────────────────────────

void ClockDisplay::drawClock(int hour, int minute, int second) {
    if (!_faceDrawn) { drawFace(); _faceDrawn = true; }

    float secAngle  = second * 6.0f;
    float minAngle  = minute * 6.0f + second * 0.1f;
    float hourAngle = (hour % 12) * 30.0f + minute * 0.5f;

    if (_lastS >= 0) {
        float oldSec  = _lastS * 6.0f;
        float oldMin  = _lastM * 6.0f + _lastS * 0.1f;
        float oldHour = (_lastH % 12) * 30.0f + _lastM * 0.5f;

        eraseHand(oldSec,          95, 1);
        eraseHand(oldSec + 180.0f, 20, 1);
        eraseHand(oldHour,         55, 7);
        eraseHand(oldMin,          80, 5);
    }

    // Always redraw hour and min BEFORE the second hand so the second hand
    // is visually on top and repairs any gaps left by erasing it.
    drawHand(hourAngle, 55, 7, COL_FACE);
    drawHand(minAngle,  80, 5, COL_FACE);
    drawHand(secAngle,  95, 1, COL_SEC);

    float tailRad = (secAngle + 90.0f) * (float)M_PI / 180.0f;
    _tft.drawLine(CX, CY,
                  CX + (int)(20 * cosf(tailRad)),
                  CY + (int)(20 * sinf(tailRad)),
                  COL_SEC);

    _tft.fillCircle(CX, CY, 3, COL_ACCENT);

    // Date label — redrawn every tick because hand-erase strokes pass through this area.
    // Erase old text first by reprinting it in background colour, then draw new text.
    {
        time_t now = time(nullptr);
        struct tm *ti = localtime(&now);
        char dateBuf[12];
        strftime(dateBuf, sizeof(dateBuf), "%a %d %b", ti);   // e.g. "Mon 19 Jul"

        static constexpr int DATE_Y = CY + 38;   // text baseline, just below centre
        int16_t x1, y1; uint16_t tw, th;

        _tft.setFont(&FreeSans9pt7b);
        _tft.setTextSize(1);

        bool dateChanged = (strcmp(dateBuf, _dateBuf) != 0);
        if (dateChanged && _dateBuf[0]) {
            // Erase old date only when it actually changes (once per day)
            _tft.getTextBounds(_dateBuf, 0, 0, &x1, &y1, &tw, &th);
            _tft.setTextColor(COL_BG);
            _tft.setCursor(CX - (int16_t)(tw / 2), DATE_Y);
            _tft.print(_dateBuf);
        }

        // Always redraw — repairs any pixels erased by the second hand
        _tft.getTextBounds(dateBuf, 0, 0, &x1, &y1, &tw, &th);
        _tft.setTextColor(0x7BEF);
        _tft.setCursor(CX - (int16_t)(tw / 2), DATE_Y);
        _tft.print(dateBuf);

        _tft.setFont(nullptr);
        if (dateChanged) memcpy(_dateBuf, dateBuf, sizeof(dateBuf));
    }

    _lastH = hour; _lastM = minute; _lastS = second;
}
