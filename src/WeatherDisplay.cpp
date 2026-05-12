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
    // Outer ring
    _tft.drawCircle(cx, cy, r, 0x4208);
    // N tick
    _tft.fillRect(cx - 1, cy - r + 1, 3, 3, COL_FACE);

    // Needle points FROM the wind source (meteorological convention).
    // deg=0 → from North → tip at top of circle.
    float rad = (deg - 90.0f) * (float)M_PI / 180.0f;
    int tx = cx + (int)((r - 2) * cosf(rad));
    int ty = cy + (int)((r - 2) * sinf(rad));
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
        // Shouldn't normally be called without data, but handle gracefully.
        _tft.setFont(&FreeSans9pt7b);
        _tft.setTextColor(0x7BEF);
        _tft.setTextSize(1);
        _tft.setCursor(CX - 30, CY);
        _tft.print("No data");
        return;
    }

    // Helper: print a string centred at y using current font/colour.
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
    _tft.drawLine(CX - 44, 103, CX + 44, 103, 0x4208);

    // ── Wind: compass + speed + direction ────────────────────────────────────
    // Compass rose left of centre; speed/dir text to the right.
    drawWindCompass(72, 122, 14, wd.windDeg);

    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_FACE);
    snprintf(buf, sizeof(buf), "%.1f kn", wd.windSpeedMs * 1.94384f);
    _tft.setCursor(95, 117);
    _tft.print(buf);

    _tft.setTextColor(COL_ACCENT);
    snprintf(buf, sizeof(buf), "%s", degToCompass(wd.windDeg));
    _tft.setCursor(95, 133);
    _tft.print(buf);

    // ── Pressure ─────────────────────────────────────────────────────────────
    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_FACE);
    snprintf(buf, sizeof(buf), "%.0f hPa", wd.pressureHPa);
    centre(buf, 151);

    // ── Humidity ─────────────────────────────────────────────────────────────
    snprintf(buf, sizeof(buf), "%.0f%% RH", wd.humidity);
    centre(buf, 167);

    // ── Separator ─────────────────────────────────────────────────────────────
    _tft.drawLine(CX - 44, 176, CX + 44, 176, 0x4208);

    // ── Description (amber, truncated to ~18 chars to stay inside bezel) ─────
    _tft.setTextColor(COL_ACCENT);
    strncpy(buf, wd.description.c_str(), 20);
    buf[20] = '\0';
    centre(buf, 191);
}
