#include "WebUI.h"
#include "WebPages.h"

namespace WebUI {

WebServer server(80);
static bool _up = false;

// ── Shared page chrome ───────────────────────────────────────────────────────

void pageHead(String &p, const __FlashStringHelper *title,
              const __FlashStringHelper *css,
              const __FlashStringHelper *extraHead) {
    p += F("<!DOCTYPE html><html><head>"
           "<meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>");
    p += title;
    p += F("</title>");
    if (extraHead) p += extraHead;
    p += F("<style>");
    p += css;
    p += F("</style></head><body>");
}

void redirect(const __FlashStringHelper *location) {
    server.sendHeader(F("Location"), String(location));
    server.send(303);
}

// ── Wiring ───────────────────────────────────────────────────────────────────

void begin() {
    WebPages::registerHome    (server);
    WebPages::registerStatus  (server);
    WebPages::registerLocation(server);
    WebPages::registerCalib   (server);
    WebPages::registerImages  (server);

    // Captive-portal behaviour: anything unknown bounces to the home page.
    server.onNotFound([]() {
        server.sendHeader(F("Location"), F("/"));
        server.send(302);
    });

    server.begin();
    _up = true;
}

void loopOnce() {
    if (_up) server.handleClient();
}

bool up() { return _up; }

}  // namespace WebUI
