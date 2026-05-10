#pragma once

// ── Display (GC9A01 240×240 circular, SPI) ───────────────────────────────────
// HUZZAH32 board SPI bus: SCK=GPIO5 (pin "SCK"), MOSI=GPIO18 (pin "MOSI")
// TFT_SCLK / TFT_MOSI are for documentation — the driver uses SPI.begin() internally.
#define TFT_SCLK   5   // board pin "SCK"  — wire display CLK  here
#define TFT_MOSI  18   // board pin "MOSI" — wire display MOSI here
#define TFT_CS    15   // board pin "D15"  — wire display CS   here
#define TFT_DC    13   // board pin "D13"  — wire display DC   here
#define TFT_RST    4   // board pin "A5"   — wire display RST  here
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

// 28BYJ-48 half-step: 64 steps/motor-rev × 64 internal gear = 4096 steps/rev on output shaft
#define STEPS_PER_REV  4096

// ── WiFi config portal ────────────────────────────────────────────────────────
#define AP_SSID    "TjofiaWX-Setup"
#define AP_PASS    ""             // leave empty for open AP
#define MDNS_NAME  "TjofiaWX"    // accessible as http://TjofiaWX.local on home WiFi

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
#define NVS_WIND_STEPS   "wind_steps"
#define NVS_PRES_STEPS   "pres_steps"

// ── Calibration reference positions ──────────────────────────────────────────
#define CAL_WIND_KT      (10.0f * 1.94384f)   // 10 m/s expressed in knots
#define CAL_PRES_HPA     1000.0f

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

// ── Firmware identity ────────────────────────────────────────────────────────
#define FW_VERSION  "1.0"

// ── Display colours (RGB565) ─────────────────────────────────────────────────
#define COL_BG      0x0000   // black
#define COL_FACE    0xFFFF   // white
#define COL_ACCENT  0xFD20   // amber
#define COL_SEC     0xF800   // red second hand
#define COL_ERROR   0xF800
#define COL_OK      0x07E0   // green
