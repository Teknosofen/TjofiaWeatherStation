#pragma once
#include <Arduino.h>

class ClockDisplay;
class Settings;

// Network bring-up and the always-on configuration access point.
//
// Two distinct portals are involved:
//   * WiFiManager's own portal, used once to collect home-WiFi credentials
//     (and the OpenWeatherMap key) when none are stored.
//   * Our own soft-AP + captive DNS, brought up after the station connects so
//     the configuration pages stay reachable even off the home network.
namespace NetPortal {

// Blocks until the station connects or WIFI_TIMEOUT_S elapses. On success it
// also starts the persistent AP, the captive DNS and mDNS, and hands the HTTP
// routes to WebUI::begin(). owmKey is filled from NVS (and from the portal
// form if the user typed one).
bool connect(ClockDisplay &display, Settings &settings, String &owmKey);

// Call from loop() — services captive-portal DNS queries.
void loopOnce();

bool apUp();

// Clear stored WiFi credentials (the caller restarts).
void forgetWifi();

}  // namespace NetPortal
