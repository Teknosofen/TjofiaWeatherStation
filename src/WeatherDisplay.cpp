#include "WeatherDisplay.h"
#include <math.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

WeatherDisplay::WeatherDisplay(int8_t cs, int8_t rst)
    : BaseDisplay(cs, rst) {}

// ── Helpers ───────────────────────────────────────────────────────────────────

const char *WeatherDisplay::degToCompass(int deg) {
    static const char *dirs[16] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return dirs[((deg + 11) / 22) % 16];
}

void WeatherDisplay::drawWindCompass(int cx, int cy, int r, int deg) {
    _tft.drawCircle(cx, cy, r + 1, 0x630C);   // outer — medium grey (shadow)
    _tft.drawCircle(cx, cy, r,     COL_FACE);  // middle — white (highlight)
    _tft.drawCircle(cx, cy, r - 1, 0xC618);   // inner — light grey (fade)

    // Cardinal labels — built-in 6×8 font, placed just inside the ring.
    _tft.setFont(nullptr);
    _tft.setTextSize(1);
    _tft.setTextColor(COL_SEC);            // N is red
    _tft.setCursor(cx - 3, cy - r + 2);   _tft.print("N");
    _tft.setTextColor(COL_FACE);           // S, E, W are white
    _tft.setCursor(cx - 3, cy + r - 9);   _tft.print("S");
    _tft.setCursor(cx + r - 7, cy - 4);   _tft.print("E");
    _tft.setCursor(cx - r + 1, cy - 4);   _tft.print("W");

    // Needle FROM wind source: deg=0 → from North → tip at top.
    float rad = (deg - 90.0f) * (float)M_PI / 180.0f;
    int tx = cx + (int)((r - 4) * cosf(rad));
    int ty = cy + (int)((r - 4) * sinf(rad));
    int bx = cx - (int)((r / 2) * cosf(rad));
    int by = cy - (int)((r / 2) * sinf(rad));
    _tft.drawLine(bx, by, tx, ty, COL_ACCENT);
    _tft.fillCircle(tx, ty, 2, COL_ACCENT);
}

// ── Public API ────────────────────────────────────────────────────────────────

void WeatherDisplay::update(const WeatherData &wd) {
    _tft.fillScreen(COL_BG);
    drawBezel();

    if (!wd.valid) {
        _tft.setFont(&FreeSans9pt7b);
        _tft.setTextColor(0x7BEF);
        _tft.setTextSize(1);
        _tft.setCursor(CX - 30, CY);
        _tft.print("No data");
        return;
    }

    auto centre = [&](const char *s, int y) {
        int16_t x1, y1; uint16_t tw, th;
        _tft.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
        _tft.setCursor(CX - (int16_t)(tw / 2), y);
        _tft.print(s);
    };

    char buf[48];

    // ── Temperature (large, amber) ────────────────────────────────────────────
    _tft.setFont(&FreeSansBold18pt7b);
    _tft.setTextSize(1);
    _tft.setTextColor(COL_ACCENT);
    snprintf(buf, sizeof(buf), "%.1f\xB0" "C", wd.tempC);
    centre(buf, 72);

    // ── Feels like (small, dim) ───────────────────────────────────────────────
    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextColor(0x7BEF);
    snprintf(buf, sizeof(buf), "feels %.1f\xB0" "C", wd.feelsLikeC);
    centre(buf, 92);

    // ── Separator ─────────────────────────────────────────────────────────────
    _tft.drawLine(CX - 44, 101, CX + 44, 101, 0x4208);

    // ── Wind: compass (cx=58, cy=135, r=30) + speed + bearing text ───────────
    drawWindCompass(58, 135, 30, wd.windDeg);

    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_FACE);
    snprintf(buf, sizeof(buf), "%.1f m/s", wd.windSpeedMs);
    _tft.setCursor(96, 130);
    _tft.print(buf);

    _tft.setTextColor(COL_ACCENT);
    snprintf(buf, sizeof(buf), "%s", degToCompass(wd.windDeg));
    _tft.setCursor(96, 146);
    _tft.print(buf);

    // ── Pressure ─────────────────────────────────────────────────────────────
    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_FACE);
    snprintf(buf, sizeof(buf), "%.0f mBar", wd.pressureHPa);
    centre(buf, 178);

    // ── Humidity ─────────────────────────────────────────────────────────────
    snprintf(buf, sizeof(buf), "%.0f%% RH", wd.humidity);
    centre(buf, 193);

    // ── Separator ─────────────────────────────────────────────────────────────
    _tft.drawLine(CX - 44, 202, CX + 44, 202, 0x4208);

    // ── Description ───────────────────────────────────────────────────────────
    _tft.setTextColor(COL_ACCENT);
    strncpy(buf, wd.description.c_str(), 20);
    buf[20] = '\0';
    centre(buf, 215);
}
