#include "BaseDisplay.h"
#include <LittleFS.h>
#include <Fonts/FreeSans9pt7b.h>

BaseDisplay::BaseDisplay(int8_t cs, int8_t rst)
    : _tft(rst, TFT_DC, cs) {}

bool BaseDisplay::showImage(const char *path) {
    File f = LittleFS.open(path, "r");
    if (!f || f.size() != 115200) { if (f) f.close(); return false; }
    DIYables_TFT_GC9A01_Round::Frame frame = {{0, 0}, {239, 239}};
    _tft.setFrame(frame);
    _tft.beginWrite();
    uint8_t buf[512];
    while (f.available()) {
        int n = f.read(buf, sizeof(buf));
        if (n > 0) _tft.writeContinue(buf, (size_t)n);
    }
    _tft.endWrite();
    f.close();
    return true;
}

bool BaseDisplay::begin() {
    _tft.begin();
    _tft.setRotation(2);       
    if (TFT_BL >= 0) {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }
    _tft.fillScreen(COL_BG);
    return true;
}

void BaseDisplay::drawBezel() {
    _tft.drawCircle(CX, CY, R + 5, 0x0841);
    _tft.drawCircle(CX, CY, R + 4, 0x1082);
    _tft.drawCircle(CX, CY, R + 3, 0x2104);
    _tft.drawCircle(CX, CY, R + 2, 0x4208);
    _tft.drawCircle(CX, CY, R + 1, 0x630C);
    _tft.drawCircle(CX, CY, R,     0x8410);
    _tft.drawCircle(CX, CY, R - 1, 0xC618);
    _tft.drawCircle(CX, CY, R - 2, COL_FACE);
}

void BaseDisplay::showSplash(const String &version, const String &buildDate) {
    _tft.fillScreen(COL_BG);
    drawBezel();

    _tft.setFont(&FreeSans9pt7b);
    _tft.setTextSize(1);

    auto centre = [&](const char *s, int y, uint16_t col) {
        int16_t x1, y1; uint16_t tw, th;
        _tft.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
        _tft.setTextColor(col);
        _tft.setCursor(CX - tw / 2, y);
        _tft.print(s);
    };

    centre("Teknosofen",       110, COL_FACE);
    centre(version.c_str(),    150, COL_FACE);
    centre(buildDate.c_str(),  170, 0x7BEF);
}

void BaseDisplay::showStatus(const String &line1, const String &line2) {
    _tft.fillScreen(COL_BG);
    drawBezel();

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
}

void BaseDisplay::showError(const String &msg) {
    _tft.fillScreen(COL_BG);
    drawBezel();

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
}

void BaseDisplay::showAPMode(const String &ssid) {
    _tft.fillScreen(COL_BG);
    drawBezel();

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
}
