#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <time.h>

#include "config.h"
#include "DisplayManager.h"
#include "Instruments.h"
#include "WeatherClient.h"

// ── Module instances ──────────────────────────────────────────────────────────
static DisplayManager display;
static Instruments    instruments;
static WeatherClient  weather;
static Preferences    prefs;

// ── State machine ─────────────────────────────────────────────────────────────
enum class State {
    BOOT,
    WIFI_SETUP,       // AP config portal active
    SYNCING_TIME,
    LOCATING,
    FETCHING_WEATHER,
    RUNNING,
    ERROR
};

static State     state        = State::BOOT;
static String    errorMsg;
static GeoInfo   geoInfo;
static WeatherData weatherData;
static String    owmKey;
static unsigned long lastWeatherMs = 0;
static unsigned long lastClockMs   = 0;
static bool      timeReady = false;

// ── WiFiManager config portal ─────────────────────────────────────────────────

// Called by WiFiManager when the user saves the config portal form.
static char      owmBuf[41] = "";
static bool      configChanged = false;

static void onSaveConfig() { configChanged = true; }

static bool startWifi() {
    // Load previously saved OWM key
    prefs.begin(NVS_NS, false);
    String saved = prefs.getString(NVS_OWM_KEY, "");
    strncpy(owmBuf, saved.c_str(), sizeof(owmBuf) - 1);
    prefs.end();

    WiFiManagerParameter owmParam("owm_key", "OpenWeatherMap API Key", owmBuf, 40);

    WiFiManager wm;
    wm.setSaveConfigCallback(onSaveConfig);
    wm.addParameter(&owmParam);
    wm.setConfigPortalTimeout(WIFI_TIMEOUT_S);
    wm.setTitle("Tjofia Weather Station");

    display.showAPMode(AP_SSID);

    bool connected;
    if (strlen(AP_PASS) > 0) {
        connected = wm.autoConnect(AP_SSID, AP_PASS);
    } else {
        connected = wm.autoConnect(AP_SSID);
    }

    if (configChanged) {
        strncpy(owmBuf, owmParam.getValue(), sizeof(owmBuf) - 1);
        prefs.begin(NVS_NS, false);
        prefs.putString(NVS_OWM_KEY, owmBuf);
        prefs.end();
    }
    owmKey = String(owmBuf);
    return connected;
}

// ── NTP / time helpers ────────────────────────────────────────────────────────

static bool syncTime(int utcOffsetSec) {
    configTime(utcOffsetSec, 0, NTP_SERVER1, NTP_SERVER2);
    // Wait up to 10 s for sync
    time_t now = 0;
    for (int i = 0; i < 20 && now < 100000; i++) {
        delay(500);
        now = time(nullptr);
    }
    return (now > 100000);
}

static void getLocalTime(int &h, int &m, int &s) {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    h = t.tm_hour;
    m = t.tm_min;
    s = t.tm_sec;
}

// ── m/s  →  knots ─────────────────────────────────────────────────────────────
static inline float msToKnots(float ms) { return ms * 1.94384f; }

// ── Arduino setup/loop ────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    instruments.begin();

    if (!display.begin()) {
        Serial.println("Display init failed");
        // continue without display
    }

    display.showStatus("Tjofia WX", "Starting...");
    delay(800);

    state = State::WIFI_SETUP;
}

void loop() {
    switch (state) {

    // ── WIFI_SETUP ────────────────────────────────────────────────────────────
    case State::WIFI_SETUP: {
        bool ok = startWifi();
        if (!ok) {
            errorMsg = "WiFi timeout.\nRestarting...";
            state = State::ERROR;
            break;
        }
        Serial.printf("Connected to WiFi, IP: %s\n", WiFi.localIP().toString().c_str());
        display.showStatus("WiFi OK", WiFi.localIP().toString());
        delay(1000);
        state = State::LOCATING;
        break;
    }

    // ── LOCATING ─────────────────────────────────────────────────────────────
    case State::LOCATING: {
        display.showStatus("Finding", "location...");

        // Try cached location first
        prefs.begin(NVS_NS, true);
        float cachedLat = prefs.getFloat(NVS_LAT, 0);
        float cachedLon = prefs.getFloat(NVS_LON, 0);
        String cachedTz = prefs.getString(NVS_TZ, "");
        prefs.end();

        bool locOk = false;
        if (cachedLat != 0 || cachedLon != 0) {
            geoInfo.lat      = cachedLat;
            geoInfo.lon      = cachedLon;
            geoInfo.timezone = cachedTz;
            geoInfo.valid    = true;
            locOk            = true;
            Serial.printf("Using cached location: %.4f, %.4f\n",
                          geoInfo.lat, geoInfo.lon);
        }

        // Always try a fresh fix (updates if moved)
        GeoInfo fresh;
        if (weather.fetchLocation(fresh)) {
            geoInfo = fresh;
            prefs.begin(NVS_NS, false);
            prefs.putFloat(NVS_LAT, geoInfo.lat);
            prefs.putFloat(NVS_LON, geoInfo.lon);
            prefs.putString(NVS_TZ, geoInfo.timezone);
            prefs.end();
            locOk = true;
            Serial.printf("Location: %s, %s (%.4f, %.4f) TZ offset %d h\n",
                          geoInfo.city.c_str(), geoInfo.country.c_str(),
                          geoInfo.lat, geoInfo.lon, geoInfo.utcOffset / 3600);
        }

        if (!locOk) {
            Serial.println("Geolocation failed, using UTC");
            geoInfo.utcOffset = 0;
            geoInfo.lat = 0; geoInfo.lon = 0;
        }

        state = State::SYNCING_TIME;
        break;
    }

    // ── SYNCING_TIME ─────────────────────────────────────────────────────────
    case State::SYNCING_TIME: {
        display.showStatus("Syncing", "time...");
        timeReady = syncTime(geoInfo.utcOffset);
        if (!timeReady) {
            Serial.println("NTP sync failed");
            // non-fatal: keep running, show --- for time
        }
        state = State::FETCHING_WEATHER;
        break;
    }

    // ── FETCHING_WEATHER ─────────────────────────────────────────────────────
    case State::FETCHING_WEATHER: {
        display.showStatus("Fetching", "weather...");
        if (!owmKey.isEmpty() && geoInfo.valid) {
            bool ok = weather.fetchWeather(geoInfo.lat, geoInfo.lon,
                                           owmKey, weatherData);
            if (ok) {
                Serial.printf("Weather: %.1f°C  %.0f hPa  wind %.1f m/s\n",
                              weatherData.tempC, weatherData.pressureHPa,
                              weatherData.windSpeedMs);
                display.setTemperature(weatherData.tempC);
                instruments.setWindSpeed(msToKnots(weatherData.windSpeedMs));
                instruments.setPressure(weatherData.pressureHPa);
                instruments.idle();
            } else {
                Serial.println("Weather fetch failed");
            }
        } else {
            Serial.println("No OWM key or location — skipping weather");
        }
        lastWeatherMs = millis();
        state = State::RUNNING;
        break;
    }

    // ── RUNNING ───────────────────────────────────────────────────────────────
    case State::RUNNING: {
        unsigned long now = millis();

        // Clock — update every second
        if (now - lastClockMs >= 1000) {
            lastClockMs = now;
            if (timeReady) {
                int h, m, s;
                getLocalTime(h, m, s);
                display.drawClock(h, m, s);
            }
        }

        // Weather — re-fetch on interval
        if (now - lastWeatherMs >= WEATHER_INTERVAL_MS) {
            state = State::FETCHING_WEATHER;
        }

        // Re-locate every few hours if we had no initial fix
        // (ip-api rate-limit: 45 req/min, so we are very conservative)
        break;
    }

    // ── ERROR ─────────────────────────────────────────────────────────────────
    case State::ERROR: {
        display.showError(errorMsg);
        Serial.println("Fatal: " + errorMsg);
        delay(5000);
        ESP.restart();
        break;
    }
    }
}
