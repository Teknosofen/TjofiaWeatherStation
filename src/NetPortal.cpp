#include "NetPortal.h"
#include "WebUI.h"
#include "Settings.h"
#include "ClockDisplay.h"
#include "config.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>

namespace NetPortal {
namespace {

DNSServer   dns;
bool        apRunning = false;

WiFiManager wm;
char        owmBuf[41] = "";
WiFiManagerParameter owmParam("owm_key", "OpenWeatherMap API Key", owmBuf, 40);
Settings   *cfg = nullptr;   // set by connect(), used by the save callback

void onSaveConfig() {
    strncpy(owmBuf, owmParam.getValue(), sizeof(owmBuf) - 1);
    if (cfg) cfg->saveOwmKey(owmBuf);
}

// Our own soft-AP, independent of WiFiManager, so it is always visible.
void startPersistentAP() {
    WiFi.softAPdisconnect(false);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    bool ok = (strlen(AP_PASS) > 0)
        ? WiFi.softAP(AP_SSID, AP_PASS)
        : WiFi.softAP(AP_SSID);
    Serial.printf("AP %s: %s  192.168.4.1\n", ok ? "up" : "FAILED", AP_SSID);

    // Redirect every hostname to 192.168.4.1 (captive-portal behaviour).
    dns.start(53, "*", IPAddress(192, 168, 4, 1));

    WebUI::begin();
    apRunning = true;

    // Also reachable as http://TjofiaWX.local on the home network.
    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("mDNS: http://%s.local\n", MDNS_NAME);
    } else {
        Serial.println("mDNS start failed");
    }
}

}  // namespace

bool connect(ClockDisplay &display, Settings &settings, String &owmKey) {
    cfg = &settings;

    String saved = settings.loadOwmKey();
    strncpy(owmBuf, saved.c_str(), sizeof(owmBuf) - 1);
    owmKey = String(owmBuf);

    wm.setSaveConfigCallback(onSaveConfig);
    wm.addParameter(&owmParam);
    wm.setConfigPortalTimeout(WIFI_TIMEOUT_S);
    wm.setTitle("Tjofia Weather Station");

    display.showAPMode(AP_SSID);
    bool connected = (strlen(AP_PASS) > 0)
        ? wm.autoConnect(AP_SSID, AP_PASS)
        : wm.autoConnect(AP_SSID);

    // The portal may have collected a key after the initial NVS read.
    owmKey = String(owmBuf);

    if (connected) startPersistentAP();
    return connected;
}

void loopOnce() {
    if (apRunning) dns.processNextRequest();
}

bool apUp() { return apRunning; }

void forgetWifi() { wm.resetSettings(); }

}  // namespace NetPortal
