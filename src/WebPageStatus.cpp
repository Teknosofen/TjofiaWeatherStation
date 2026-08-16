#include "WebUI.h"
#include "WebPages.h"
#include "AppContext.h"
#include "config.h"

// /wx — read-only status table plus the OpenWeatherMap key field.

namespace {

const char CSS[] PROGMEM =
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
    ".btn{display:inline-block;padding:8px 14px;background:#1fa3ec;color:#fff;"
        "border:none;border-radius:4px;cursor:pointer;text-decoration:none}"
    ".box{background:#f9f9f9;border:1px solid #ddd;border-radius:6px;"
        "padding:12px;margin:14px 0}"
    "input[type=text]{width:100%;padding:8px;box-sizing:border-box;"
        "margin:4px 0 8px;border:1px solid #ccc;border-radius:4px}";

const char REFRESH_HEAD[] PROGMEM = "<meta http-equiv='refresh' content='30'>";

const char *const COMPASS_PTS[16] = {
    "N","NNE","NE","ENE","E","ESE","SE","SSE",
    "S","SSW","SW","WSW","W","WNW","NW","NNW"
};

String buildPage() {
    String p;
    p.reserve(3200);
    WebUI::pageHead(p, F("Tjofia WX"), FPSTR(CSS), FPSTR(REFRESH_HEAD));
    p += F("<h1>&#127781; Tjofia WX</h1><table>");

    auto row = [&](const char *label, const String &val) {
        p += F("<tr><td>"); p += label;
        p += F("</td><td>"); p += val; p += F("</td></tr>");
    };

    char buf[48];

    app.clock.formatHms(buf, sizeof(buf));
    row("Time", buf);

    if (app.geo.valid) {
        row("Location", app.geo.city + ", " + app.geo.country);
        snprintf(buf, sizeof(buf), "%.4f&deg;N, %.4f&deg;E", app.geo.lat, app.geo.lon);
        row("Lat / Lon", buf);
    } else {
        row("Location", "---");
        row("Lat / Lon", "---");
    }

    if (app.wx.valid) {
        snprintf(buf, sizeof(buf), "%.1f &deg;C", app.wx.tempC);
        row("Temperature", buf);
        snprintf(buf, sizeof(buf), "%.1f m/s &nbsp;(%.1f kn)",
                 app.wx.windSpeedMs, AppContext::msToKnots(app.wx.windSpeedMs));
        row("Wind speed", buf);

        const char *cp = COMPASS_PTS[((app.wx.windDeg + 11) / 22) % 16];
        snprintf(buf, sizeof(buf), "%d&deg; &mdash; %s", app.wx.windDeg, cp);
        row("Wind direction", buf);

        snprintf(buf, sizeof(buf), "%.0f hPa&nbsp;/&nbsp;mBar", app.wx.pressureHPa);
        row("Pressure", buf);
        row("Conditions", app.wx.description);

        if (app.lastWeatherMs > 0) {
            unsigned long age = (millis() - app.lastWeatherMs) / 1000UL;
            if      (age < 60)   snprintf(buf, sizeof(buf), "%u s ago",   (unsigned)age);
            else if (age < 3600) snprintf(buf, sizeof(buf), "%u min ago", (unsigned)(age / 60));
            else                 snprintf(buf, sizeof(buf), "%u h ago",   (unsigned)(age / 3600));
            row("Last fetch", buf);
        }
    } else {
        row("Weather", "not yet available");
    }

    snprintf(buf, sizeof(buf), "%d / %d steps", app.instruments.getWdirSteps(), WDIR_MAX_STEPS);
    row("Wind dir", buf);
    snprintf(buf, sizeof(buf), "%d / %d steps", app.instruments.getPresSteps(), PRES_MAX_STEPS);
    row("Pres. gauge", buf);
    snprintf(buf, sizeof(buf), "%.0f mV &nbsp;(%.1f kn)  fs=%.0f mV",
             app.speedMeter.getCurrentMv(),
             app.speedMeter.getCurrentMv() / app.speedMeter.getFullScaleMv() * SpeedMeter::MAX_KNOTS,
             app.speedMeter.getFullScaleMv());
    row("Speed meter", buf);
    row("Cal. mode", app.calMode ? "<b style='color:#c00'>ACTIVE</b>" : "off");

    p += F("</table>"
           "<p class='note'>Auto-refreshes every 30&nbsp;s</p>");

    p += F("<div class='box'><b>OpenWeatherMap API Key</b>"
           "<form method='POST' action='/save' style='margin-top:8px'>"
           "<input type='text' name='owm_key' placeholder='paste key here' value='");
    p += app.settings.loadOwmKey();
    p += F("'><button class='btn' type='submit'>Save key</button></form></div>"
           "<p><a class='btn' href='/'>&#8592; Back</a></p>"
           "</body></html>");
    return p;
}

void handlePage() {
    WebUI::server.send(200, "text/html", buildPage());
}

}  // namespace

void WebPages::registerStatus(WebServer &s) {
    s.on("/wx", HTTP_GET, handlePage);
}
