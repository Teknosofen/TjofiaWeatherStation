#include "WebUI.h"
#include "WebPages.h"
#include "AppContext.h"
#include "config.h"

// /location — Leaflet map for pinning the station's coordinates.
// A pinned location overrides IP geolocation for weather lookups; the UTC
// offset still comes from ip-api either way.

namespace {

const char CSS[] PROGMEM =
    "body{font-family:sans-serif;max-width:640px;margin:12px auto;padding:0 12px}"
    "h1{font-size:1.2em}"
    "#map{height:350px;border-radius:6px;margin:10px 0;border:1px solid #ccc}"
    ".btn{display:inline-block;padding:8px 14px;background:#1fa3ec;color:#fff;"
         "border:none;border-radius:4px;cursor:pointer;text-decoration:none;margin:3px 2px}"
    ".red{background:#c00}.grn{background:#2a2}"
    "p.h{font-size:.85em;color:#555;margin:.4em 0}";

const char LEAFLET_HEAD[] PROGMEM =
    "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'/>"
    "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>";

// Fallback view when no position is known yet: central Sweden.
constexpr float FALLBACK_LAT = 62.0f;
constexpr float FALLBACK_LON = 15.0f;

String buildPage() {
    bool pinned = app.settings.locationPinned();

    char latS[14], lonS[14];
    snprintf(latS, sizeof(latS), "%.5f", app.geo.valid ? app.geo.lat : FALLBACK_LAT);
    snprintf(lonS, sizeof(lonS), "%.5f", app.geo.valid ? app.geo.lon : FALLBACK_LON);

    String p;
    p.reserve(3200);
    WebUI::pageHead(p, F("Tjofia WX &mdash; Location"), FPSTR(CSS), FPSTR(LEAFLET_HEAD));
    p += F("<h1>&#128205; Set Location</h1>");
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

void handlePage() {
    WebUI::server.send(200, "text/html", buildPage());
}

void handleSave() {
    WebServer &s = WebUI::server;
    if (s.hasArg("lat") && s.hasArg("lon")) {
        float lat = s.arg("lat").toFloat();
        float lon = s.arg("lon").toFloat();
        app.geo.lat   = lat;
        app.geo.lon   = lon;
        app.geo.valid = true;

        // Resolve a city name for the new coordinates.
        String newCity, newCountry;
        if (app.weather.fetchReverseGeo(lat, lon, newCity, newCountry)) {
            app.geo.city    = newCity;
            app.geo.country = newCountry;
        } else {
            app.geo.city    = "";
            app.geo.country = "";
        }

        app.settings.savePinnedCoords(lat, lon);

        Serial.printf("Location pinned: %s%s(%.5f, %.5f)\n",
                      app.geo.city.isEmpty()    ? "" : (app.geo.city + ", ").c_str(),
                      app.geo.country.isEmpty() ? "" : (app.geo.country + "  ").c_str(),
                      lat, lon);

        // Refresh weather 30 s from now so the new location shows up quickly
        // without hammering the API — the normal 10-min timer is the backstop.
        app.scheduleWeatherRefresh(30UL * 1000);
    }
    WebUI::redirect(F("/location"));
}

void handleClear() {
    app.settings.clearLocationPin();
    Serial.println("Location pin cleared — reverting to IP geolocation");
    WebUI::redirect(F("/location"));
}

}  // namespace

void WebPages::registerLocation(WebServer &s) {
    s.on("/location",       HTTP_GET,  handlePage);
    s.on("/location/save",  HTTP_POST, handleSave);
    s.on("/location/clear", HTTP_GET,  handleClear);
}
