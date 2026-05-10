#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
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

// ── Persistent AP — our own WebServer + DNS (not WiFiManager's) ──────────────
static WebServer server(80);
static DNSServer dns;
static bool      portalUp = false;

// ── State machine ─────────────────────────────────────────────────────────────
enum class State {
    BOOT, WIFI_SETUP, SYNCING_TIME, LOCATING, FETCHING_WEATHER, RUNNING, ERROR
};

static State         state        = State::BOOT;
static String        errorMsg;
static GeoInfo       geoInfo;
static WeatherData   weatherData;
static String        owmKey;
static unsigned long lastWeatherMs = 0;
static unsigned long lastClockMs   = 0;
static bool          timeReady     = false;

// ── Calibration mode ──────────────────────────────────────────────────────────
static bool calMode           = false;
static bool calModeRequested  = false;
static bool calModeRequestVal = false;

// ── WiFiManager — used only for the initial WiFi credential portal ────────────
static char              owmBuf[41] = "";
static WiFiManager       wm;
static WiFiManagerParameter owmParam("owm_key", "OpenWeatherMap API Key", owmBuf, 40);

// ── NVS helpers ───────────────────────────────────────────────────────────────

static void saveGaugePositions() {
    prefs.begin(NVS_NS, false);
    prefs.putInt(NVS_WIND_STEPS, instruments.getWindSteps());
    prefs.putInt(NVS_PRES_STEPS, instruments.getPresSteps());
    prefs.end();
}

// ── Calibration ───────────────────────────────────────────────────────────────

static void applyCalMode(bool enable) {
    calMode = enable;
    if (enable) {
        instruments.setWindSpeed(CAL_WIND_KT);
        instruments.setPressure(CAL_PRES_HPA);
    } else if (weatherData.valid) {
        instruments.setWindSpeed(weatherData.windSpeedMs * 1.94384f);
        instruments.setPressure(weatherData.pressureHPa);
    }
    instruments.idle();
    saveGaugePositions();
}

// ── Web pages ─────────────────────────────────────────────────────────────────

static String buildConfigPage() {
    prefs.begin(NVS_NS, true);
    String key = prefs.getString(NVS_OWM_KEY, "");
    prefs.end();

    String p;
    p.reserve(1200);
    p += F("<!DOCTYPE html><html><head>"
           "<meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>Tjofia WX</title>"
           "<style>"
           "body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 12px}"
           "input[type=text]{width:100%;padding:8px;box-sizing:border-box;"
                            "margin:4px 0 12px;border:1px solid #ccc;border-radius:4px}"
           ".btn{display:inline-block;padding:8px 16px;background:#1fa3ec;"
               "color:#fff;border:none;border-radius:4px;cursor:pointer;text-decoration:none}"
           ".red{background:#c00}"
           "hr{margin:16px 0}small{color:#666}"
           "</style></head><body>"
           "<h1>&#127781; Tjofia WX</h1>"
           "<h3>OpenWeatherMap API Key</h3>"
           "<form method='POST' action='/save'>"
           "<input type='text' name='owm_key' placeholder='paste key here' value='");
    p += key;
    p += F("'><button class='btn' type='submit'>Save</button></form>"
           "<hr>"
           "<a class='btn' href='/wx'>Weather Status &amp; Calibration</a>"
           "<hr><small>WiFi: ");
    p += WiFi.SSID();
    p += F("&nbsp;&nbsp;IP: ");
    p += WiFi.localIP().toString();
    p += F("</small><br><br>"
           "<a class='btn red' href='/reset'"
           " onclick=\"return confirm('Clear WiFi settings and restart?')\">"
           "Reset WiFi</a>"
           "</body></html>");
    return p;
}

static String buildStatusPage() {
    String p;
    p.reserve(2048);

    p += F("<!DOCTYPE html><html><head>"
           "<meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>Tjofia WX</title>"
           "<meta http-equiv='refresh' content='30'>"
           "<style>"
           "body{font-family:sans-serif;max-width:480px;margin:20px auto;"
                "padding:0 12px;background:#f4f4f4}"
           "h1{font-size:1.2em;margin-bottom:8px}"
           "table{width:100%;border-collapse:collapse;background:#fff;"
                 "border-radius:6px;overflow:hidden;"
                 "box-shadow:0 1px 3px rgba(0,0,0,.15)}"
           "td{padding:7px 10px;border-bottom:1px solid #eee}"
           "td:first-child{color:#555;width:42%}"
           ".cal{margin-top:16px;padding:12px;background:#fff;"
                "border:2px solid #c00;border-radius:6px}"
           ".cal h2{margin:0 0 8px;font-size:1em;color:#c00}"
           "label{cursor:pointer}"
           "input[type=checkbox]{width:18px;height:18px;"
                                "vertical-align:middle;margin-right:6px}"
           "a{color:#0066cc}"
           ".note{font-size:.8em;color:#999;margin-top:12px}"
           "</style></head><body>"
           "<h1>&#127781; Tjofia WX</h1><table>");

    auto row = [&](const char *label, const String &val) {
        p += F("<tr><td>"); p += label;
        p += F("</td><td>"); p += val; p += F("</td></tr>");
    };

    char buf[48];

    if (timeReady) {
        time_t now = time(nullptr);
        struct tm tm;
        localtime_r(&now, &tm);
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else {
        strcpy(buf, "--:--:--");
    }
    row("Time", buf);

    if (geoInfo.valid) {
        row("Location", geoInfo.city + ", " + geoInfo.country);
        snprintf(buf, sizeof(buf), "%.4f&deg;N, %.4f&deg;E", geoInfo.lat, geoInfo.lon);
        row("Lat / Lon", buf);
    } else {
        row("Location", "---");
        row("Lat / Lon", "---");
    }

    if (weatherData.valid) {
        snprintf(buf, sizeof(buf), "%.1f &deg;C", weatherData.tempC);
        row("Temperature", buf);
        snprintf(buf, sizeof(buf), "%.1f m/s &nbsp;(%.1f kn)",
                 weatherData.windSpeedMs, weatherData.windSpeedMs * 1.94384f);
        row("Wind speed", buf);
        snprintf(buf, sizeof(buf), "%.0f hPa", weatherData.pressureHPa);
        row("Pressure", buf);
        row("Conditions", weatherData.description);

        if (lastWeatherMs > 0) {
            unsigned long age = (millis() - lastWeatherMs) / 1000UL;
            if      (age < 60)   snprintf(buf, sizeof(buf), "%u s ago",   (unsigned)age);
            else if (age < 3600) snprintf(buf, sizeof(buf), "%u min ago", (unsigned)(age / 60));
            else                 snprintf(buf, sizeof(buf), "%u h ago",   (unsigned)(age / 3600));
            row("Last fetch", buf);
        }
    } else {
        row("Weather", "not yet available");
    }

    snprintf(buf, sizeof(buf), "%d / %d steps", instruments.getWindSteps(), WIND_MAX_STEPS);
    row("Wind gauge", buf);
    snprintf(buf, sizeof(buf), "%d / %d steps", instruments.getPresSteps(), PRES_MAX_STEPS);
    row("Pres. gauge", buf);
    row("Cal. mode", calMode ? "<b style='color:#c00'>ACTIVE</b>" : "off");

    p += F("</table>"
           "<div class='cal'><h2>Calibration mode</h2>"
           "<form method='POST' action='/cal'>"
           "<label><input type='checkbox' name='cal'");
    if (calMode) p += F(" checked");
    p += F(" onchange='this.form.submit()'>"
           "Move dials to reference &nbsp;(10&nbsp;m/s &nbsp;/&nbsp;1000&nbsp;hPa)"
           "</label></form>"
           "<p style='font-size:.85em;color:#666;margin:.5em 0 0'>"
           "Position is saved; motors resume here if restarted in this state."
           "</p></div>"
           "<p class='note'>Auto-refreshes every 30&nbsp;s &middot; "
           "<a href='/'>&#9881; Setup</a></p>"
           "</body></html>");
    return p;
}

static void handleRoot()       { server.send(200, "text/html", buildConfigPage()); }
static void handleStatusPage() { server.send(200, "text/html", buildStatusPage()); }

static void handleSave() {
    if (server.hasArg("owm_key")) {
        owmKey = server.arg("owm_key");
        owmKey.trim();
        prefs.begin(NVS_NS, false);
        prefs.putString(NVS_OWM_KEY, owmKey);
        prefs.end();
        Serial.printf("OWM key saved: %s\n", owmKey.c_str());
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

static void handleCalToggle() {
    bool requestOn = server.hasArg("cal");
    if (requestOn != calMode) {
        calModeRequested  = true;
        calModeRequestVal = requestOn;
    }
    server.sendHeader("Location", "/wx");
    server.send(303);
}

static void handleReset() {
    wm.resetSettings();
    server.send(200, "text/html",
        "<html><body><p>WiFi cleared. Restarting...</p></body></html>");
    delay(2000);
    ESP.restart();
}

// ── Persistent AP — started once after WiFi connects ─────────────────────────

static void startPersistentAP() {
    // Start our own soft-AP, independent of WiFiManager, so it is always visible.
    WiFi.softAPdisconnect(false);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    bool ok = (strlen(AP_PASS) > 0)
        ? WiFi.softAP(AP_SSID, AP_PASS)
        : WiFi.softAP(AP_SSID);
    Serial.printf("AP %s: %s  192.168.4.1\n", ok ? "up" : "FAILED", AP_SSID);

    // DNS: redirect every hostname → 192.168.4.1 (captive portal behaviour)
    dns.start(53, "*", IPAddress(192, 168, 4, 1));

    server.on("/",      HTTP_GET,  handleRoot);
    server.on("/save",  HTTP_POST, handleSave);
    server.on("/wx",    HTTP_GET,  handleStatusPage);
    server.on("/cal",   HTTP_POST, handleCalToggle);
    server.on("/reset", HTTP_GET,  handleReset);
    server.onNotFound([]() {
        server.sendHeader("Location", "/");
        server.send(302);
    });
    server.begin();
    portalUp = true;

    // mDNS — reachable as http://TjofiaWX.local on the home WiFi network
    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("mDNS: http://%s.local\n", MDNS_NAME);
    } else {
        Serial.println("mDNS start failed");
    }
}

// ── WiFiManager (initial credential setup only) ───────────────────────────────

static void onSaveConfig() {
    strncpy(owmBuf, owmParam.getValue(), sizeof(owmBuf) - 1);
    prefs.begin(NVS_NS, false);
    prefs.putString(NVS_OWM_KEY, owmBuf);
    prefs.end();
    owmKey = String(owmBuf);
}

static bool startWifi() {
    prefs.begin(NVS_NS, false);
    String saved = prefs.getString(NVS_OWM_KEY, "");
    strncpy(owmBuf, saved.c_str(), sizeof(owmBuf) - 1);
    prefs.end();
    owmKey = String(owmBuf);

    wm.setSaveConfigCallback(onSaveConfig);
    wm.addParameter(&owmParam);
    wm.setConfigPortalTimeout(WIFI_TIMEOUT_S);
    wm.setTitle("Tjofia Weather Station");

    display.showAPMode(AP_SSID);
    bool connected = (strlen(AP_PASS) > 0)
        ? wm.autoConnect(AP_SSID, AP_PASS)
        : wm.autoConnect(AP_SSID);

    if (connected) startPersistentAP();
    return connected;
}

// ── NTP / time helpers ────────────────────────────────────────────────────────

static bool syncTime(int utcOffsetSec) {
    configTime(utcOffsetSec, 0, NTP_SERVER1, NTP_SERVER2);
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
    h = t.tm_hour; m = t.tm_min; s = t.tm_sec;
}

static inline float msToKnots(float ms) { return ms * 1.94384f; }

// ── Arduino setup / loop ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    prefs.begin(NVS_NS, true);
    int windSteps = prefs.getInt(NVS_WIND_STEPS, 0);
    int presSteps = prefs.getInt(NVS_PRES_STEPS, 0);
    prefs.end();
    Serial.printf("Restored gauge pos: wind=%d  pres=%d steps\n", windSteps, presSteps);

    instruments.begin(windSteps, presSteps);

    if (!display.begin()) Serial.println("Display init failed");
    display.showSplash("v" FW_VERSION, __DATE__);
    delay(5000);

    state = State::WIFI_SETUP;
}

void loop() {
    if (portalUp) {
        dns.processNextRequest();
        server.handleClient();
    }

    if (calModeRequested) {
        calModeRequested = false;
        applyCalMode(calModeRequestVal);
    }

    switch (state) {

    case State::WIFI_SETUP: {
        bool ok = startWifi();
        if (!ok) {
            errorMsg = "WiFi timeout.\nRestarting...";
            state = State::ERROR;
            break;
        }
        Serial.printf("WiFi: %s  IP: %s\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        display.showStatus("WiFi OK", WiFi.localIP().toString());
        delay(1000);
        state = State::LOCATING;
        break;
    }

    case State::LOCATING: {
        display.showStatus("Finding", "location...");

        prefs.begin(NVS_NS, true);
        float cachedLat = prefs.getFloat(NVS_LAT, 0);
        float cachedLon = prefs.getFloat(NVS_LON, 0);
        String cachedTz = prefs.getString(NVS_TZ, "");
        int cachedOff   = prefs.getInt(NVS_UTC_OFF, 0);
        prefs.end();

        if (cachedLat != 0 || cachedLon != 0) {
            geoInfo.lat = cachedLat; geoInfo.lon = cachedLon;
            geoInfo.timezone = cachedTz; geoInfo.utcOffset = cachedOff;
            geoInfo.valid = true;
        }

        GeoInfo fresh;
        if (weather.fetchLocation(fresh)) {
            geoInfo = fresh;
            prefs.begin(NVS_NS, false);
            prefs.putFloat(NVS_LAT, geoInfo.lat);
            prefs.putFloat(NVS_LON, geoInfo.lon);
            prefs.putString(NVS_TZ, geoInfo.timezone);
            prefs.putInt(NVS_UTC_OFF, geoInfo.utcOffset);
            prefs.end();
            Serial.printf("Location: %s, %s (%.4f, %.4f)\n",
                          geoInfo.city.c_str(), geoInfo.country.c_str(),
                          geoInfo.lat, geoInfo.lon);
        } else if (!geoInfo.valid) {
            Serial.println("Geolocation failed — using UTC");
            geoInfo.utcOffset = 0;
        }
        state = State::SYNCING_TIME;
        break;
    }

    case State::SYNCING_TIME: {
        display.showStatus("Syncing", "time...");
        timeReady = syncTime(geoInfo.utcOffset);
        if (!timeReady) Serial.println("NTP sync failed");
        state = State::FETCHING_WEATHER;
        break;
    }

    case State::FETCHING_WEATHER: {
        display.showStatus("Fetching", "weather...");

        if (!owmKey.isEmpty() && geoInfo.valid) {
            bool ok = weather.fetchWeather(geoInfo.lat, geoInfo.lon, owmKey, weatherData);
            if (ok) {
                Serial.printf(
                    "Weather: %.1f C  feels %.1f C  "
                    "wind %.1f m/s (%.1f kn)  %.0f hPa  %s\n",
                    weatherData.tempC, weatherData.feelsLikeC,
                    weatherData.windSpeedMs,
                    weatherData.windSpeedMs * 1.94384f,
                    weatherData.pressureHPa,
                    weatherData.description.c_str());
                if (!calMode) {
                    display.setTemperature(weatherData.tempC);
                    instruments.setWindSpeed(msToKnots(weatherData.windSpeedMs));
                    instruments.setPressure(weatherData.pressureHPa);
                    instruments.idle();
                    saveGaugePositions();
                }
            } else {
                Serial.println("Weather fetch failed");
            }
        } else {
            Serial.println("Skipping weather — no OWM key or location");
        }
        lastWeatherMs = millis();
        state = State::RUNNING;
        break;
    }

    case State::RUNNING: {
        unsigned long now = millis();
        if (now - lastClockMs >= 1000) {
            lastClockMs = now;
            if (timeReady) {
                int h, m, s;
                getLocalTime(h, m, s);
                display.drawClock(h, m, s);
            }
        }
        if (now - lastWeatherMs >= WEATHER_INTERVAL_MS) {
            state = State::FETCHING_WEATHER;
        }
        break;
    }

    case State::ERROR: {
        display.showError(errorMsg);
        Serial.println("Fatal: " + errorMsg);
        delay(5000);
        ESP.restart();
        break;
    }
    }
}
