#include "WebUI.h"
#include "WebPages.h"
#include "AppContext.h"
#include "NetPortal.h"
#include <WiFi.h>

// Landing page (/) plus the three small actions that belong to no other page:
// saving the OpenWeatherMap key, toggling calibration mode, clearing WiFi.

namespace {

const char CSS[] PROGMEM =
    "body{font-family:sans-serif;max-width:300px;margin:20px auto;padding:0 12px}"
    ".btn{display:block;padding:10px 16px;background:#1fa3ec;color:#fff;"
        "border:none;border-radius:4px;cursor:pointer;text-decoration:none;"
        "margin:6px 0;text-align:center}"
    ".red{background:#c00}"
    "hr{margin:16px 0}small{color:#666}";

String buildPage() {
    String p;
    p.reserve(900);
    WebUI::pageHead(p, F("Tjofia WX"), FPSTR(CSS));
    p += F("<h1>&#127781; Tjofia WX</h1>"
           "<a class='btn' href='/wx'>&#9729; Weather</a>"
           "<a class='btn' href='/location'>&#128205; Set Location</a>"
           "<a class='btn' href='/calib'>&#9881; Calibration</a>"
           "<a class='btn' href='/images'>&#128444; Images</a>"
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

void handlePage() {
    WebUI::server.send(200, "text/html", buildPage());
}

void handleSaveKey() {
    if (WebUI::server.hasArg("owm_key")) {
        app.owmKey = WebUI::server.arg("owm_key");
        app.owmKey.trim();
        app.settings.saveOwmKey(app.owmKey);
        Serial.printf("OWM key saved: %s\n", app.owmKey.c_str());
    }
    WebUI::redirect(F("/wx"));
}

// Deferred to loop(): moving the needles blocks for seconds, and the browser
// should have its redirect long before that.
void handleCalToggle() {
    app.requestCalMode(WebUI::server.hasArg("cal"));
    WebUI::redirect(F("/calib"));
}

void handleResetWifi() {
    NetPortal::forgetWifi();
    WebUI::server.send(200, "text/html",
        "<html><body><p>WiFi cleared. Restarting...</p></body></html>");
    delay(2000);
    ESP.restart();
}

}  // namespace

void WebPages::registerHome(WebServer &s) {
    s.on("/",      HTTP_GET,  handlePage);
    s.on("/save",  HTTP_POST, handleSaveKey);
    s.on("/cal",   HTTP_POST, handleCalToggle);
    s.on("/reset", HTTP_GET,  handleResetWifi);
}
