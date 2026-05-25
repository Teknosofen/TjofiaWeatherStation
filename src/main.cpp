#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>

#include "config.h"
#include "ClockDisplay.h"
#include "WeatherDisplay.h"
#include "Instruments.h"
#include "WeatherClient.h"
#include "SpeedMeter.h"

// ── Module instances ──────────────────────────────────────────────────────────
static ClockDisplay   clockDisp(TFT_CS);      // primary: owns RST pulse
static WeatherDisplay weatherDisp(TFT2_CS);   // secondary: rst=-1 by default
static Instruments    instruments;
static SpeedMeter     speedMeter(PWM_SPEED_PIN);
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
    prefs.putInt(NVS_WIND_STEPS, instruments.getWdirSteps());
    prefs.putInt(NVS_PRES_STEPS, instruments.getPresSteps());
    prefs.end();
}

// ── Calibration ───────────────────────────────────────────────────────────────

static void applyCalMode(bool enable) {
    calMode = enable;
    if (enable) {
        instruments.setWindDir(CAL_WDIR_DEG);                    // North
        instruments.setPressure(CAL_PRES_HPA);
        speedMeter.setKnots(CAL_WIND_MS * 1.94384f);             // 10 m/s in knots
    } else if (weatherData.valid) {
        instruments.setWindDir(weatherData.windDeg);
        instruments.setPressure(weatherData.pressureHPa);
        speedMeter.setKnots(weatherData.windSpeedMs * 1.94384f);
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
           "<a class='btn' href='/wx'>Weather Status</a>"
           "&nbsp;<a class='btn' href='/location'>&#128205; Set Location</a>"
           "&nbsp;<a class='btn' href='/calib'>&#9881; Calibration</a>"
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

    snprintf(buf, sizeof(buf), "%d / %d steps", instruments.getWdirSteps(), WDIR_MAX_STEPS);
    row("Wind dir", buf);
    snprintf(buf, sizeof(buf), "%d / %d steps", instruments.getPresSteps(), PRES_MAX_STEPS);
    row("Pres. gauge", buf);
    snprintf(buf, sizeof(buf), "%.0f mV &nbsp;(%.1f kn)  fs=%.0f mV",
             speedMeter.getCurrentMv(),
             speedMeter.getCurrentMv() / speedMeter.getFullScaleMv() * SpeedMeter::MAX_KNOTS,
             speedMeter.getFullScaleMv());
    row("Speed meter", buf);
    row("Cal. mode", calMode ? "<b style='color:#c00'>ACTIVE</b>" : "off");

    p += F("</table>"
           "<p class='note'>Auto-refreshes every 30&nbsp;s &middot; "
           "<a href='/'>&#8592; Back</a>"
           " &middot; <a href='/calib'>&#9881; Calibration</a></p>"
           "</body></html>");
    return p;
}

// ── Location pin page ─────────────────────────────────────────────────────────

static String buildLocationPage() {
    prefs.begin(NVS_NS, true);
    bool pinned = prefs.getBool(NVS_LOC_PINNED, false);
    prefs.end();

    char latS[14], lonS[14];
    float lat = geoInfo.valid ? geoInfo.lat : 62.0f;
    float lon = geoInfo.valid ? geoInfo.lon : 15.0f;
    snprintf(latS, sizeof(latS), "%.5f", lat);
    snprintf(lonS, sizeof(lonS), "%.5f", lon);

    String p;
    p.reserve(3200);
    p += F("<!DOCTYPE html><html><head>"
           "<meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>Tjofia WX &mdash; Location</title>"
           "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
           "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
           "<style>"
           "body{font-family:sans-serif;max-width:640px;margin:12px auto;padding:0 12px}"
           "h1{font-size:1.2em}"
           "#map{height:350px;border-radius:6px;margin:10px 0;border:1px solid #ccc}"
           ".btn{display:inline-block;padding:8px 14px;background:#1fa3ec;color:#fff;"
                "border:none;border-radius:4px;cursor:pointer;text-decoration:none;margin:3px 2px}"
           ".red{background:#c00}.grn{background:#2a2}"
           "p.h{font-size:.85em;color:#555;margin:.4em 0}"
           "</style></head><body>"
           "<h1>&#128205; Set Location</h1>");
    if (pinned) {
        p += F("<p style='color:#2a2;font-weight:bold'>&#10003; Location is pinned "
               "&#8212; IP geolocation is ignored for weather data.</p>");
    }
    p += F("<p class='h'>Click the map to place a pin, or tap <b>Use my GPS</b>."
           " The map requires internet access.</p>"
           "<button class='btn' onclick='useGPS()'>&#127968; Use my GPS</button>"
           "<div id='map'></div>"
           "<form method='POST' action='/location/save'>"
           "<input type='hidden' id='lat' name='lat' value='"); p += latS;
    p += F("'><input type='hidden' id='lon' name='lon' value='"); p += lonS;
    p += F("'><p id='pin' class='h'>Current: "); p += latS; p += F("&deg;N, "); p += lonS;
    p += F("&deg;E</p>"
           "<button class='btn grn' type='submit'>&#128190; Save &amp; pin</button>"
           "&nbsp;<a class='btn red' href='/location/clear'>&#10060; Clear pin (use auto)</a>"
           "&nbsp;<a class='btn' href='/'>&#8592; Back</a>"
           "</form>"
           "<script>"
           "var lt="); p += latS; p += F(",ln="); p += lonS;
    p += F(";"
           "var map=L.map('map').setView([lt,ln],10);"
           "L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',"
           "{attribution:'&copy; <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a>',"
           "maxZoom:19}).addTo(map);"
           "var mk=L.marker([lt,ln],{draggable:true}).addTo(map);"
           "mk.on('dragend',function(){place(mk.getLatLng().lat,mk.getLatLng().lng)});"
           "map.on('click',function(e){place(e.latlng.lat,e.latlng.lng)});"
           "function place(a,b){"
             "mk.setLatLng([a,b]);"
             "document.getElementById('lat').value=a.toFixed(5);"
             "document.getElementById('lon').value=b.toFixed(5);"
             "document.getElementById('pin').textContent="
               "'Pin: '+a.toFixed(4)+'\\u00b0N, '+b.toFixed(4)+'\\u00b0E';}"
           "function useGPS(){"
             "if(!navigator.geolocation){alert('Geolocation not available in this browser');return;}"
             "navigator.geolocation.getCurrentPosition("
               "function(p){map.setView([p.coords.latitude,p.coords.longitude],14);"
                           "place(p.coords.latitude,p.coords.longitude);},"
               "function(e){alert('Could not get position: '+e.message);});}"
           "</script></body></html>");
    return p;
}

static void handleLocationPage() {
    server.send(200, "text/html", buildLocationPage());
}

static void handleLocationSave() {
    if (server.hasArg("lat") && server.hasArg("lon")) {
        float lat = server.arg("lat").toFloat();
        float lon = server.arg("lon").toFloat();
        geoInfo.lat   = lat;
        geoInfo.lon   = lon;
        geoInfo.valid = true;

        // Resolve city name from new coordinates
        String newCity, newCountry;
        if (weather.fetchReverseGeo(lat, lon, newCity, newCountry)) {
            geoInfo.city    = newCity;
            geoInfo.country = newCountry;
        } else {
            geoInfo.city    = "";
            geoInfo.country = "";
        }

        prefs.begin(NVS_NS, false);
        prefs.putFloat(NVS_LAT, lat);
        prefs.putFloat(NVS_LON, lon);
        prefs.putBool(NVS_LOC_PINNED, true);
        prefs.end();

        Serial.printf("Location pinned: %s%s(%.5f, %.5f)\n",
                      geoInfo.city.isEmpty()    ? "" : (geoInfo.city + ", ").c_str(),
                      geoInfo.country.isEmpty() ? "" : (geoInfo.country + "  ").c_str(),
                      lat, lon);
    }
    server.sendHeader("Location", "/location");
    server.send(303);
}

static void handleLocationClear() {
    prefs.begin(NVS_NS, false);
    prefs.remove(NVS_LOC_PINNED);
    prefs.end();
    Serial.println("Location pin cleared — reverting to IP geolocation");
    server.sendHeader("Location", "/location");
    server.send(303);
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
    server.sendHeader("Location", "/calib");
    server.send(303);
}

static void handleReset() {
    wm.resetSettings();
    server.send(200, "text/html",
        "<html><body><p>WiFi cleared. Restarting...</p></body></html>");
    delay(2000);
    ESP.restart();
}

// ── Calibration page ─────────────────────────────────────────────────────────

static String buildCalibPage() {
    char fsMvStr[12], curMvStr[12];
    snprintf(fsMvStr, sizeof(fsMvStr), "%.0f", speedMeter.getFullScaleMv());
    snprintf(curMvStr, sizeof(curMvStr), "%.0f", speedMeter.getCurrentMv());

    String p;
    p.reserve(2000);
    p += F("<!DOCTYPE html><html><head>"
           "<meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>Tjofia WX &mdash; Calibration</title>"
           "<style>"
           "body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 12px}"
           "h1{font-size:1.2em}h2{font-size:1em;margin:0 0 6px}"
           "input[type=number]{width:100%;padding:8px;box-sizing:border-box;"
                              "margin:4px 0 10px;border:1px solid #ccc;border-radius:4px}"
           ".btn{display:inline-block;padding:8px 16px;background:#1fa3ec;"
               "color:#fff;border:none;border-radius:4px;cursor:pointer;text-decoration:none}"
           ".box{background:#f9f9f9;border:1px solid #ddd;border-radius:6px;"
                "padding:14px;margin:14px 0}"
           "label{cursor:pointer}"
           "input[type=checkbox]{width:18px;height:18px;vertical-align:middle;margin-right:6px}"
           "small{color:#666}"
           "</style></head><body>"
           "<h1>&#9881; Calibration</h1>"
           "<div class='box'>"
           "<h2>Stepper gauges</h2>"
           "<form method='POST' action='/cal'>"
           "<label><input type='checkbox' name='cal'");
    if (calMode) p += F(" checked");
    p += F(" onchange='this.form.submit()'>"
           "Move dials to reference &nbsp;&mdash; North (0&deg;) &bull; 10&nbsp;m/s &bull; 1000&nbsp;hPa"
           "</label></form>"
           "<p><small>Position saved to NVS. If power is cycled while active, "
           "motors resume at the reference position.</small></p>"
           "</div>"
           "<div class='box'>"
           "<h2>Wind speed meter (PWM &rarr; GPIO22)</h2>"
           "<p><small>Range: 0&ndash;30 kn. Set the output voltage (mV) that drives"
           " the needle to full-scale. Current output: ");
    p += curMvStr;
    p += F(" mV.</small></p>"
           "<form method='POST' action='/calib/save'>"
           "<label>Full-scale voltage (mV)</label>"
           "<input type='number' name='fs_mv' min='50' max='3300' step='1' value='");
    p += fsMvStr;
    p += F("'>"
           "<button class='btn' type='submit'>Save</button>"
           "</form></div>"
           "<a class='btn' href='/'>&#8592; Back</a>"
           "</body></html>");
    return p;
}

static void handleCalibPage() { server.send(200, "text/html", buildCalibPage()); }

static void handleCalibSave() {
    if (server.hasArg("fs_mv")) {
        float mv = server.arg("fs_mv").toFloat();
        if (mv >= 50.0f && mv <= 3300.0f) {
            speedMeter.setFullScaleMv(mv);
            // Re-apply current wind speed so needle moves immediately to reflect new cal
            if (calMode) {
                speedMeter.setKnots(CAL_WIND_KT);
            } else if (weatherData.valid) {
                speedMeter.setKnots(weatherData.windSpeedMs * 1.94384f);
            }
            prefs.begin(NVS_NS, false);
            prefs.putFloat(NVS_PWM_FS_MV, mv);
            prefs.end();
            Serial.printf("Speed meter full-scale saved: %.0f mV\n", mv);
        }
    }
    server.sendHeader("Location", "/calib");
    server.send(303);
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

    server.on("/",               HTTP_GET,  handleRoot);
    server.on("/save",           HTTP_POST, handleSave);
    server.on("/wx",             HTTP_GET,  handleStatusPage);
    server.on("/cal",            HTTP_POST, handleCalToggle);
    server.on("/reset",          HTTP_GET,  handleReset);
    server.on("/location",       HTTP_GET,  handleLocationPage);
    server.on("/location/save",  HTTP_POST, handleLocationSave);
    server.on("/location/clear", HTTP_GET,  handleLocationClear);
    server.on("/calib",          HTTP_GET,  handleCalibPage);
    server.on("/calib/save",     HTTP_POST, handleCalibSave);
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

    clockDisp.showAPMode(AP_SSID);
    weatherDisp.showAPMode(AP_SSID);
    bool connected = (strlen(AP_PASS) > 0)
        ? wm.autoConnect(AP_SSID, AP_PASS)
        : wm.autoConnect(AP_SSID);

    if (connected) startPersistentAP();
    return connected;
}

// ── NTP / time helpers ────────────────────────────────────────────────────────

static bool syncTime(int utcOffsetSec) {
    // configTime() on ESP32 generates an invalid POSIX tz string when
    // daylightOffset_sec = 0 (e.g. "UTC-1UTC-11"), which the C library
    // rejects and falls back to UTC.  Build the string ourselves instead.
    //
    // POSIX sign convention is inverted vs common usage:
    //   "UTC-1" = one hour east of UTC  (= UTC+1)
    //   "UTC+5" = five hours west of UTC (= UTC-5)
    int h = utcOffsetSec / 3600;
    int m = abs((utcOffsetSec % 3600) / 60);
    char tzPosix[20];
    if (m == 0)
        snprintf(tzPosix, sizeof(tzPosix), "UTC%+d",       -h);
    else
        snprintf(tzPosix, sizeof(tzPosix), "UTC%+d:%02d",  -h, m);
    Serial.printf("NTP sync: offset %+d s → POSIX \"%s\"\n", utcOffsetSec, tzPosix);
    configTzTime(tzPosix, NTP_SERVER1, NTP_SERVER2);
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
    int   wdirSteps = prefs.getInt  (NVS_WIND_STEPS, 0);
    int   presSteps = prefs.getInt  (NVS_PRES_STEPS, 0);
    float pwmFsMv   = prefs.getFloat(NVS_PWM_FS_MV,  SpeedMeter::DEFAULT_FS_MV);
    prefs.end();
    Serial.printf("Restored gauge pos: wdir=%d  pres=%d steps\n", wdirSteps, presSteps);
    Serial.printf("Speed meter full-scale: %.0f mV\n", pwmFsMv);

    instruments.begin(wdirSteps, presSteps);
    speedMeter.begin(pwmFsMv);

    // Primary begin() drives the shared RST line; secondary begin() skips it.
    if (!clockDisp.begin())   Serial.println("Display 1 init failed");
    if (!weatherDisp.begin()) Serial.println("Display 2 init failed");
    clockDisp.showSplash("v" FW_VERSION, __DATE__);
    weatherDisp.showSplash("v" FW_VERSION, __DATE__);
    delay(2000);

    // Self-test: motors sweep ±30°; PWM rides 150 → 200 → 100 → 150 mV simultaneously.
    static const int ST = 341;
    speedMeter.setMillivolts(150.0f);
    for (int i = 0; i < ST; i++) {
        instruments.stepBoth(+1, 3);
        speedMeter.setMillivolts(150.0f + 50.0f * i / (ST - 1));   // 150 → 200 mV
    }
    for (int i = 0; i < ST; i++) {
        instruments.stepBoth(-1, 3);
        speedMeter.setMillivolts(200.0f - 100.0f * i / (ST - 1));  // 200 → 100 mV
    }
    instruments.idle();
    speedMeter.setMillivolts(150.0f);   // rest at mid-scale until weather arrives

    delay(2000);

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
        clockDisp.showStatus(WiFi.SSID(), WiFi.localIP().toString());
        weatherDisp.showStatus(WiFi.SSID(), WiFi.localIP().toString());
        delay(1000);
        state = State::LOCATING;
        break;
    }

    case State::LOCATING: {
        clockDisp.showStatus("Finding", "location...");
        weatherDisp.showStatus("Finding", "location...");

        prefs.begin(NVS_NS, true);
        float cachedLat = prefs.getFloat(NVS_LAT, 0);
        float cachedLon = prefs.getFloat(NVS_LON, 0);
        String cachedTz = prefs.getString(NVS_TZ, "");
        int   cachedOff = prefs.getInt(NVS_UTC_OFF, 0);
        bool  pinned    = prefs.getBool(NVS_LOC_PINNED, false);
        prefs.end();

        if (cachedLat != 0 || cachedLon != 0) {
            geoInfo.lat = cachedLat; geoInfo.lon = cachedLon;
            geoInfo.timezone = cachedTz; geoInfo.utcOffset = cachedOff;
            geoInfo.valid = true;
        }

        GeoInfo fresh;
        if (weather.fetchLocation(fresh)) {
            if (!pinned) {
                // Auto mode — use ip-api lat/lon and city
                geoInfo.lat     = fresh.lat;
                geoInfo.lon     = fresh.lon;
                geoInfo.city    = fresh.city;
                geoInfo.country = fresh.country;
            }
            // Always take fresh UTC offset regardless of pin state
            geoInfo.timezone  = fresh.timezone;
            geoInfo.utcOffset = fresh.utcOffset;
            geoInfo.valid = true;

            prefs.begin(NVS_NS, false);
            if (!pinned) {
                prefs.putFloat(NVS_LAT, geoInfo.lat);
                prefs.putFloat(NVS_LON, geoInfo.lon);
            }
            prefs.putString(NVS_TZ, geoInfo.timezone);
            prefs.putInt(NVS_UTC_OFF, geoInfo.utcOffset);
            prefs.end();

            Serial.printf("Location: %s (%.4f, %.4f)  tz offset %+d s%s\n",
                          pinned ? "[pinned]" : (geoInfo.city + ", " + geoInfo.country).c_str(),
                          geoInfo.lat, geoInfo.lon, geoInfo.utcOffset,
                          pinned ? " (UTC offset from ip-api)" : "");
        } else if (!geoInfo.valid) {
            Serial.println("Geolocation failed — using UTC");
            geoInfo.utcOffset = 0;
        }
        state = State::SYNCING_TIME;
        break;
    }

    case State::SYNCING_TIME: {
        clockDisp.showStatus("Syncing", "time...");
        weatherDisp.showStatus("Syncing", "time...");
        timeReady = syncTime(geoInfo.utcOffset);
        if (!timeReady) Serial.println("NTP sync failed");
        state = State::FETCHING_WEATHER;
        break;
    }

    case State::FETCHING_WEATHER: {
        clockDisp.showStatus("Fetching", "weather...");
        weatherDisp.showStatus("Fetching", "weather...");

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
                    clockDisp.setTemperature(weatherData.tempC);
                    weatherDisp.update(weatherData);
                    instruments.setWindDir(weatherData.windDeg);
                    instruments.setPressure(weatherData.pressureHPa);
                    instruments.idle();
                    speedMeter.setKnots(msToKnots(weatherData.windSpeedMs));
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
                clockDisp.drawClock(h, m, s);
            }
        }
        if (now - lastWeatherMs >= WEATHER_INTERVAL_MS) {
            state = State::FETCHING_WEATHER;
        }
        break;
    }

    case State::ERROR: {
        clockDisp.showError(errorMsg);
        weatherDisp.showError(errorMsg);
        Serial.println("Fatal: " + errorMsg);
        delay(5000);
        ESP.restart();
        break;
    }
    }
}
