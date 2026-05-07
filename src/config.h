#pragma once

// ── Display (GC9A01 240×240 circular, VSPI) ──────────────────────────────────
// These pins avoid all motor pins (14,27,32,33 and 25,26,16,17)
#define TFT_SCLK   5
#define TFT_MOSI  18
#define TFT_CS    23
#define TFT_DC    13
#define TFT_RST    4
#define TFT_BL    -1   // set to GPIO pin if backlight is PWM-controlled

// ── Motor 1  (Wind-speed gauge) ───────────────────────────────────────────────
#define MOTOR1_P1  14
#define MOTOR1_P2  27
#define MOTOR1_P3  32
#define MOTOR1_P4  33

// ── Motor 2  (Pressure gauge) ─────────────────────────────────────────────────
#define MOTOR2_P1  25
#define MOTOR2_P2  26
#define MOTOR2_P3  16
#define MOTOR2_P4  17

// 28BYJ-48 in half-step mode: 64 * 8 * (64/1) gear = 4096 steps / rev
#define STEPS_PER_REV  4096

// ── WiFi config portal ────────────────────────────────────────────────────────
#define AP_SSID  "TjofiaWX-Setup"
#define AP_PASS  ""          // leave empty for open AP

// ── NTP ───────────────────────────────────────────────────────────────────────
#define NTP_SERVER1  "pool.ntp.org"
#define NTP_SERVER2  "time.nist.gov"

// ── External APIs ─────────────────────────────────────────────────────────────
// IP geolocation — free, no key
#define GEO_URL  "http://ip-api.com/json"
// OpenWeatherMap current weather (metric units)
#define OWM_URL  "https://api.openweathermap.org/data/2.5/weather"

// ── Preferences keys (NVS) ───────────────────────────────────────────────────
#define NVS_NS           "tjofia"
#define NVS_OWM_KEY      "owm_key"
#define NVS_TZ           "timezone"
#define NVS_LAT          "lat"
#define NVS_LON          "lon"

// ── Timing (milliseconds) ────────────────────────────────────────────────────
#define WEATHER_INTERVAL_MS  (10UL * 60 * 1000)   // 10 min
#define GEO_RETRY_MS         (5UL  * 60 * 1000)   //  5 min
#define WIFI_TIMEOUT_S       180                   // AP portal timeout

// ── Gauge physical limits ────────────────────────────────────────────────────
// Motor 1 – wind speed:  0–60 knots mapped to 0–(3/4 rev)
#define WIND_MIN_KT      0.0f
#define WIND_MAX_KT     60.0f
#define WIND_MAX_STEPS  (STEPS_PER_REV * 3 / 4)

// Motor 2 – pressure:  960–1040 hPa mapped to full scale
#define PRES_MIN_HPA   960.0f
#define PRES_MAX_HPA  1040.0f
#define PRES_MAX_STEPS (STEPS_PER_REV * 3 / 4)

// ── Display colours (RGB565) ─────────────────────────────────────────────────
#define COL_BG      0x0000   // black
#define COL_FACE    0xFFFF   // white
#define COL_ACCENT  0xFD20   // amber
#define COL_SEC     0xF800   // red second hand
#define COL_ERROR   0xF800
#define COL_OK      0x07E0   // green
