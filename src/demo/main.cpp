#include <Arduino.h>
#include <math.h>
#include <DIYables_TFT_Round.h>
#include <Fonts/FreeSans9pt7b.h>
#include "config.h"
#include "Stepper28BYJ.h"

static Stepper28BYJ wind(MOTOR1_P1, MOTOR1_P2, MOTOR1_P3, MOTOR1_P4);
static Stepper28BYJ pres(MOTOR2_P1, MOTOR2_P2, MOTOR2_P3, MOTOR2_P4);
static DIYables_TFT_GC9A01_Round tft(TFT_RST, TFT_DC, TFT_CS);

static const int STEP_MS = 3;
static const int CX = 120, CY = 120, R = 112;

// ── Display helpers ───────────────────────────────────────────────────────────

static void centreText(const char *s, int y, uint16_t col) {
    int16_t x1, y1; uint16_t tw, th;
    tft.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
    tft.setTextColor(col);
    tft.setCursor(CX - tw / 2, y);
    tft.print(s);
}

static void drawClockFace() {
    tft.fillScreen(0x0000);
    tft.drawCircle(CX, CY, R,     0xFFFF);
    tft.drawCircle(CX, CY, R - 1, 0xFFFF);

    for (int i = 0; i < 60; i++) {
        float rad = (i * 6 - 90) * (float)M_PI / 180.0f;
        bool isHour = (i % 5 == 0);
        int r0 = isHour ? R - 12 : R - 6;
        tft.drawLine(
            CX + (int)(R  * cosf(rad)), CY + (int)(R  * sinf(rad)),
            CX + (int)(r0 * cosf(rad)), CY + (int)(r0 * sinf(rad)),
            isHour ? (uint16_t)0xFFFF : (uint16_t)0x7BEF
        );
    }

    // Static hands at 10:10 — the classic showcase position
    auto hand = [&](float deg, int len, int thick, uint16_t col) {
        float rad = (deg - 90.0f) * (float)M_PI / 180.0f;
        int x2 = CX + (int)(len * cosf(rad));
        int y2 = CY + (int)(len * sinf(rad));
        for (int d = -(thick / 2); d <= thick / 2; d++) {
            float rp = rad + (float)M_PI_2;
            tft.drawLine(CX + (int)(d * cosf(rp)), CY + (int)(d * sinf(rp)),
                         x2 + (int)(d * cosf(rp)), y2 + (int)(d * sinf(rp)), col);
        }
    };

    hand(300.0f, 55, 5, 0xFFFF);   // hour  — 10:00
    hand( 60.0f, 80, 3, 0xFFFF);   // minute — :10
    hand(  0.0f, 95, 1, 0xF800);   // second — :00 (red)

    // Second hand tail
    float tailRad = (180.0f - 90.0f) * (float)M_PI / 180.0f;
    tft.drawLine(CX, CY,
                 CX + (int)(20 * cosf(tailRad)),
                 CY + (int)(20 * sinf(tailRad)), 0xF800);

    tft.fillCircle(CX, CY, 4, 0xFD20);  // amber centre dot
}

static void drawHelloWorld() {
    tft.fillScreen(0x0000);
    tft.setFont(&FreeSans9pt7b);
    tft.setTextSize(1);

    // Five colours, evenly spaced vertically on the 240px screen
    const uint16_t cols[]  = { 0xF800, 0xFD20, 0x07E0, 0x07FF, 0xFFFF };
    const char    *names[] = { "Red",  "Amber", "Green", "Cyan", "White" };

    for (int i = 0; i < 5; i++) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Hello World — %s", names[i]);
        centreText(buf, 70 + i * 26, cols[i]);
    }
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("=== Motor + Display demo ===");
    Serial.println("Needles must be at physical zero before running.");

    tft.begin();
    tft.fillScreen(0x0000);

    // ── Phase 1: clock face ───────────────────────────────────────────────────
    Serial.println("\n-- Phase 1: clock face --");
    drawClockFace();
    delay(4000);

    // ── Phase 2: hello world in five colours ─────────────────────────────────
    Serial.println("\n-- Phase 2: Hello World --");
    drawHelloWorld();
    delay(4000);

    // ── Phase 3: motor sanity check ──────────────────────────────────────────
    Serial.println("\n-- Phase 3: motor check --");
    wind.rotate(WIND_MAX_STEPS, +1, STEP_MS);
    pres.rotate(PRES_MAX_STEPS, +1, STEP_MS);
    wind.off(); pres.off();
    delay(400);
    wind.rotate(WIND_MAX_STEPS, -1, STEP_MS);
    pres.rotate(PRES_MAX_STEPS, -1, STEP_MS);
    wind.off(); pres.off();

    Serial.println("\nDemo complete. Needles should be at zero.");
}

void loop() {}
