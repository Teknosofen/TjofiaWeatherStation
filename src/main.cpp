// Tjofia Weather Station — boot sequence and top-level state machine.
//
// Everything else lives in a module:
//   AppContext      shared state + the actions the web layer triggers
//   NetPortal       WiFi, persistent AP, captive DNS, mDNS
//   WebUI/WebPage*  HTTP interface
//   Settings        NVS persistence
//   ImageStore      LittleFS .raw gallery
//   Slideshow       weather/photo rotation on the second display
//   GaugeCalWizard  two-point stepper calibration
//   TimeService     NTP + local clock
//   Instruments     stepper gauges;  SpeedMeter: PWM wind-speed meter
//   ClockDisplay / WeatherDisplay: the two GC9A01 round displays

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "AppContext.h"
#include "NetPortal.h"
#include "WebUI.h"

AppContext app;

// ── Boot-time state machine ──────────────────────────────────────────────────

enum class State {
    BOOT, WIFI_SETUP, SYNCING_TIME, LOCATING, FETCHING_WEATHER, RUNNING, ERROR
};

static State  state = State::BOOT;
static String errorMsg;
static unsigned long lastClockMs = 0;

// ── Boot sequence helpers ────────────────────────────────────────────────────

// Restore persisted gauge positions and calibration coefficients.
static void restoreInstruments() {
    int wdirSteps, presSteps;
    app.settings.loadGaugeSteps(wdirSteps, presSteps);
    Serial.printf("Restored gauge pos: wdir=%d  pres=%d steps\n", wdirSteps, presSteps);
    app.instruments.begin(wdirSteps, presSteps);

    float pwmFsMv = app.settings.loadSpeedFullScaleMv(SpeedMeter::DEFAULT_FS_MV);
    app.speedMeter.begin(pwmFsMv);
    Serial.printf("Speed meter full-scale: %.0f mV\n", pwmFsMv);

    // A stored gain far from the factory value usually means a mis-clicked
    // wizard rather than an unusual dial — warn instead of silently obeying.
    struct { bool wdir; float factory; const char *unit; } gauges[] = {
        { true,  (float)WDIR_MAX_STEPS / (WDIR_MAX_DEG - WDIR_MIN_DEG),  "deg" },
        { false, (float)PRES_MAX_STEPS / (PRES_MAX_HPA - PRES_MIN_HPA),  "hPa" },
    };
    for (auto &g : gauges) {
        float zero, gain;
        if (!app.settings.loadGaugeCal(g.wdir, zero, gain)) continue;
        if (g.wdir) app.instruments.setWdirCalibration(zero, gain);
        else        app.instruments.setPresCalibration(zero, gain);
        Serial.printf("%s cal: zero=%.2f  gain=%.4f steps/%s  (factory: %.4f)\n",
                      g.wdir ? "Wdir" : "Pres", zero, gain, g.unit, g.factory);
        if (gain < g.factory * 0.4f || gain > g.factory * 2.5f)
            Serial.printf("  *** %s gain looks wrong — use /calib to Reset if the dial "
                          "reads incorrectly ***\n", g.wdir ? "Wdir" : "Pres");
    }
}

// Weather display at boot: pinned image → first stored image → Teknosofen splash.
static void showBootImage() {
    app.bootImg = app.settings.loadBootImage();

    if (!app.bootImg.isEmpty() && app.weatherDisp.showImage(app.bootImg.c_str())) {
        app.weatherBootPath = app.bootImg;
        Serial.printf("Boot image (pinned): %s\n", app.bootImg.c_str());
        return;
    }

    // No pin (or the pinned file is gone/corrupt) — fall back to the first
    // stored image that actually renders.
    app.images.forEach([](const String &name, size_t) {
        if (!app.weatherBootPath.isEmpty()) return;
        String path = ImageStore::path(name);
        if (app.weatherDisp.showImage(path.c_str())) {
            app.weatherBootPath = path;
            Serial.printf("Boot image (auto): %s\n", path.c_str());
        }
    });
    if (!app.weatherBootPath.isEmpty()) return;

    // weatherBootPath stays "" → refreshWeatherBoot() re-shows the splash.
    app.weatherDisp.showSplash("v" FW_VERSION, __DATE__);
}

// Motors sweep ±30° while the PWM output rides 150 → 200 → 100 → 150 mV.
// Net displacement is zero, so the restored NVS positions stay accurate.
static void selfTest() {
    static const int ST = 341;   // ≈ 30° of output-shaft rotation
    app.speedMeter.setMillivolts(150.0f);
    for (int i = 0; i < ST; i++) {
        app.instruments.stepBoth(+1, 3);
        app.speedMeter.setMillivolts(150.0f + 50.0f * i / (ST - 1));    // 150 → 200 mV
    }
    for (int i = 0; i < ST; i++) {
        app.instruments.stepBoth(-1, 3);
        app.speedMeter.setMillivolts(200.0f - 100.0f * i / (ST - 1));   // 200 → 100 mV
    }
    app.instruments.idle();
    app.speedMeter.setMillivolts(150.0f);   // rest at mid-scale until weather arrives
}

// ── State handlers ───────────────────────────────────────────────────────────

static void stepLocating() {
    app.clockDisp.showStatus("Finding", "location...");
    app.refreshWeatherBoot();

    float  cachedLat, cachedLon;
    String cachedTz;
    int    cachedOff;
    bool   pinned;
    app.settings.loadLocation(cachedLat, cachedLon, cachedTz, cachedOff, pinned);

    if (cachedLat != 0 || cachedLon != 0) {
        app.geo.lat       = cachedLat;
        app.geo.lon       = cachedLon;
        app.geo.timezone  = cachedTz;
        app.geo.utcOffset = cachedOff;
        app.geo.valid     = true;
    }

    GeoInfo fresh;
    if (app.weather.fetchLocation(fresh)) {
        if (!pinned) {
            app.geo.lat     = fresh.lat;
            app.geo.lon     = fresh.lon;
            app.geo.city    = fresh.city;
            app.geo.country = fresh.country;
        }
        // The fresh UTC offset is always taken, pinned or not.
        app.geo.timezone  = fresh.timezone;
        app.geo.utcOffset = fresh.utcOffset;
        app.geo.valid     = true;

        if (!pinned) app.settings.saveCoords(app.geo.lat, app.geo.lon);
        app.settings.saveTimezone(app.geo.timezone, app.geo.utcOffset);

        Serial.printf("Location: %s (%.4f, %.4f)  tz offset %+d s%s\n",
                      pinned ? "[pinned]" : (app.geo.city + ", " + app.geo.country).c_str(),
                      app.geo.lat, app.geo.lon, app.geo.utcOffset,
                      pinned ? " (UTC offset from ip-api)" : "");
    } else if (!app.geo.valid) {
        Serial.println("Geolocation failed — using UTC");
        app.geo.utcOffset = 0;
    }
}

static void stepFetchWeather() {
    app.clockDisp.showStatus("Fetching", "weather...");
    app.refreshWeatherBoot();

    bool fetched = false;
    if (!app.owmKey.isEmpty() && app.geo.valid) {
        fetched = app.weather.fetchWeather(app.geo.lat, app.geo.lon, app.owmKey, app.wx);
        if (!fetched) Serial.println("Weather fetch failed");
    } else {
        Serial.println("Skipping weather — no OWM key or location");
    }

    if (!fetched) {
        if (!app.wx.valid) app.weatherDisp.showStatus("No weather data", "Check setup");
        return;
    }

    Serial.printf("Weather: %.1f C  feels %.1f C  "
                  "wind %.1f m/s (%.1f kn) from %d deg  %.0f hPa  %s\n",
                  app.wx.tempC, app.wx.feelsLikeC,
                  app.wx.windSpeedMs, AppContext::msToKnots(app.wx.windSpeedMs),
                  app.wx.windDeg, app.wx.pressureHPa, app.wx.description.c_str());

    // Calibration mode and the wizard both own the gauges — leave them alone.
    if (app.calMode || app.gaugeCal.active()) return;

    app.weatherBootPath = "";   // hand the weather display over to live data
    app.instruments.setWindDir(app.wx.windDeg);
    app.instruments.setPressure(app.wx.pressureHPa);
    app.instruments.idle();
    app.speedMeter.setKnots(AppContext::msToKnots(app.wx.windSpeedMs));
    app.saveGaugePositions();

    unsigned long now = millis();
    app.slideshow.showWeather(app.wx, now);
    app.slideshow.start(now);
}

static void stepRunning() {
    unsigned long now = millis();

    if (now - lastClockMs >= 1000) {
        lastClockMs = now;
        if (app.clock.ready()) {
            int h, m, s;
            app.clock.local(h, m, s);
            app.clockDisp.drawClock(h, m, s);
        }
    }

    if (now - app.lastWeatherMs >= WEATHER_INTERVAL_MS) {
        state = State::FETCHING_WEATHER;
        return;
    }

    if (!app.calMode && !app.gaugeCal.active())
        app.slideshow.tick(now, app.wx);
}

// ── Arduino entry points ─────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    if (!app.images.begin())
        Serial.println("LittleFS mount failed — images unavailable");

    restoreInstruments();
    app.slideshow.setIntervalSec(app.settings.loadSlideSec(Slideshow::DEFAULT_SEC));

    // The primary begin() drives the shared RST line; the secondary skips it.
    if (!app.clockDisp.begin())   Serial.println("Display 1 init failed");
    if (!app.weatherDisp.begin()) Serial.println("Display 2 init failed");
    app.clockDisp.showSplash("v" FW_VERSION, __DATE__);
    showBootImage();
    delay(2000);

    selfTest();
    delay(2000);

    state = State::WIFI_SETUP;
}

void loop() {
    NetPortal::loopOnce();
    WebUI::loopOnce();

    bool wantCalMode;
    if (app.takePendingCalMode(wantCalMode)) app.applyCalMode(wantCalMode);

    switch (state) {

    case State::WIFI_SETUP:
        if (!NetPortal::connect(app.clockDisp, app.settings, app.owmKey)) {
            errorMsg = "WiFi timeout.\nRestarting...";
            state = State::ERROR;
            break;
        }
        Serial.printf("WiFi: %s  IP: %s\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        app.clockDisp.showStatus(WiFi.SSID(), WiFi.localIP().toString());
        app.refreshWeatherBoot();
        delay(1000);
        state = State::LOCATING;
        break;

    case State::LOCATING:
        stepLocating();
        state = State::SYNCING_TIME;
        break;

    case State::SYNCING_TIME:
        app.clockDisp.showStatus("Syncing", "time...");
        app.refreshWeatherBoot();
        if (!app.clock.syncNtp(app.geo.utcOffset)) Serial.println("NTP sync failed");
        state = State::FETCHING_WEATHER;
        break;

    case State::FETCHING_WEATHER:
        stepFetchWeather();
        app.lastWeatherMs = millis();
        app.running = true;
        state = State::RUNNING;
        break;

    case State::RUNNING:
        stepRunning();
        break;

    case State::ERROR:
        app.clockDisp.showError(errorMsg);
        app.weatherDisp.showError(errorMsg);
        Serial.println("Fatal: " + errorMsg);
        delay(5000);
        ESP.restart();
        break;

    case State::BOOT:
        break;
    }
}
