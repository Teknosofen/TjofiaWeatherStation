#include "DisplayManager.h"
#include "config.h"
#include <math.h>

// Font includes from Adafruit GFX (bundled with Arduino_GFX_Library)
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

static Arduino_DataBus *makeBus() {
    return new Arduino_HWSPI(TFT_DC, TFT_CS);
}

DisplayManager::DisplayManager() {}

bool DisplayManager::begin() {
    _gfx = new Arduino_GC9A01(makeBus(), TFT_RST, 0 /* rotation 0 */);
    if (!_gfx->begin()) return false;

    if (TFT_BL >= 0) {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }

    _gfx->fillScreen(COL_BG);
    return true;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void DisplayManager::drawHand(float angleDeg, int length, int thickness, uint16_t col) {
    float rad = (angleDeg - 90.0f) * (float)M_PI / 180.0f;
    int x2 = CX + (int)(length * cosf(rad));
    int y2 = CY + (int)(length * sinf(rad));
    if (thickness <= 1) {
        _gfx->drawLine(CX, CY, x2, y2, col);
    } else {
        // draw a few parallel lines for thickness
        for (int d = -(thickness/2); d <= thickness/2; d++) {
            float rp = rad + (float)M_PI_2;
            int dx = (int)(d * cosf(rp));
            int dy = (int)(d * sinf(rp));
            _gfx->drawLine(CX + dx, CY + dy, x2 + dx, y2 + dy, col);
        }
    }
}

void DisplayManager::eraseHand(float angleDeg, int length, int thickness) {
    // Redraw over the old face tick marks rather than a plain black erase,
    // to avoid flickering. Simple approach: erase with black then redraw ticks.
    drawHand(angleDeg, length, thickness, COL_BG);
}

void DisplayManager::drawFace() {
    _gfx->drawCircle(CX, CY, R,     COL_FACE);
    _gfx->drawCircle(CX, CY, R - 1, COL_FACE);

    for (int i = 0; i < 60; i++) {
        float rad = (i * 6 - 90) * (float)M_PI / 180.0f;
        int isHour = (i % 5 == 0);
        int r0 = isHour ? R - 10 : R - 5;
        int x1 = CX + (int)(R     * cosf(rad));
        int y1 = CY + (int)(R     * sinf(rad));
        int x2 = CX + (int)(r0    * cosf(rad));
        int y2 = CY + (int)(r0    * sinf(rad));
        uint16_t col = isHour ? COL_FACE : 0x7BEF; // grey minutes
        _gfx->drawLine(x1, y1, x2, y2, col);
    }
    // Centre dot
    _gfx->fillCircle(CX, CY, 3, COL_ACCENT);
}

void DisplayManager::drawCentreText(float tempC) {
    // Clear centre area (leave room for hands)
    _gfx->fillRect(CX - 45, CY + 20, 90, 28, COL_BG);
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(COL_ACCENT);
    _gfx->setTextSize(1);

    char buf[12];
    if (tempC > -100.0f) {
        snprintf(buf, sizeof(buf), "%.1f\xB0" "C", tempC);
    } else {
        strcpy(buf, "---");
    }
    int16_t x1, y1; uint16_t tw, th;
    _gfx->getTextBounds(buf, 0, 0, &x1, &y1, &tw, &th);
    _gfx->setCursor(CX - tw / 2, CY + 40);
    _gfx->print(buf);
}

// ── Public API ────────────────────────────────────────────────────────────────

void DisplayManager::drawClock(int hour, int minute, int second) {
    // On first call draw the static face
    static bool faceDrawn = false;
    if (!faceDrawn) { drawFace(); faceDrawn = true; }

    // Erase old hands before drawing new ones
    if (_lastS >= 0) {
        eraseHand(_lastS * 6.0f,                     95, 1);
        eraseHand(_lastM * 6.0f + _lastS * 0.1f,     80, 3);
        eraseHand((_lastH % 12) * 30.0f + _lastM * 0.5f, 55, 5);
        // Restore ticks that were erased
        drawFace();
    }

    float secAngle  = second * 6.0f;
    float minAngle  = minute * 6.0f + second * 0.1f;
    float hourAngle = (hour % 12) * 30.0f + minute * 0.5f;

    drawHand(hourAngle, 55, 5, COL_FACE);
    drawHand(minAngle,  80, 3, COL_FACE);
    drawHand(secAngle,  95, 1, COL_SEC);

    // Small tail for second hand
    float tailRad = (secAngle - 90.0f + 180.0f) * (float)M_PI / 180.0f;
    _gfx->drawLine(CX, CY,
                   CX + (int)(20 * cosf(tailRad)),
                   CY + (int)(20 * sinf(tailRad)),
                   COL_SEC);

    _gfx->fillCircle(CX, CY, 3, COL_ACCENT);

    if (_lastTemp > -999) drawCentreText(_lastTemp);

    _lastH = hour; _lastM = minute; _lastS = second;
}

void DisplayManager::setTemperature(float tempC) {
    _lastTemp = tempC;
    if (_lastS >= 0) drawCentreText(tempC); // refresh immediately if clock is shown
}

void DisplayManager::showStatus(const String &line1, const String &line2) {
    _gfx->fillScreen(COL_BG);
    _gfx->drawCircle(CX, CY, R,     COL_FACE);
    _gfx->drawCircle(CX, CY, R - 1, COL_FACE);

    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(COL_FACE);
    _gfx->setTextSize(1);

    int16_t x1, y1; uint16_t tw, th;
    _gfx->getTextBounds(line1.c_str(), 0, 0, &x1, &y1, &tw, &th);
    _gfx->setCursor(CX - tw / 2, line2.isEmpty() ? CY + 6 : CY - 4);
    _gfx->print(line1);

    if (!line2.isEmpty()) {
        _gfx->getTextBounds(line2.c_str(), 0, 0, &x1, &y1, &tw, &th);
        _gfx->setCursor(CX - tw / 2, CY + 20);
        _gfx->setTextColor(COL_ACCENT);
        _gfx->print(line2);
    }
    _lastS = -1; // force face redraw next time
}

void DisplayManager::showError(const String &msg) {
    _gfx->fillScreen(COL_BG);
    _gfx->drawCircle(CX, CY, R, COL_ERROR);

    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(COL_ERROR);
    _gfx->setTextSize(1);
    _gfx->setCursor(CX - 22, CY - 10);
    _gfx->print("ERROR");

    _gfx->setTextColor(COL_FACE);
    _gfx->setFont(nullptr); // tiny default font for long messages
    _gfx->setTextSize(1);
    _gfx->setCursor(8, CY + 15);
    _gfx->setTextWrap(true);
    _gfx->print(msg);
    _lastS = -1;
}

void DisplayManager::showAPMode(const String &ssid) {
    _gfx->fillScreen(COL_BG);
    _gfx->drawCircle(CX, CY, R, 0x07FF); // cyan

    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(0x07FF);
    _gfx->setTextSize(1);

    auto centreText = [&](const String &s, int y, uint16_t col) {
        int16_t x1, y1; uint16_t tw, th;
        _gfx->getTextBounds(s.c_str(), 0, 0, &x1, &y1, &tw, &th);
        _gfx->setTextColor(col);
        _gfx->setCursor(CX - tw / 2, y);
        _gfx->print(s);
    };

    centreText("WiFi Setup", CY - 30, 0x07FF);
    centreText("Connect to:", CY - 8, COL_FACE);
    centreText(ssid, CY + 14, COL_ACCENT);
    centreText("192.168.4.1", CY + 36, 0x7BEF);
    _lastS = -1;
}
