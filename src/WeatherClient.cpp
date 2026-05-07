#include "WeatherClient.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

bool WeatherClient::fetchLocation(GeoInfo &out) {
    WiFiClient plain;
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(plain, GEO_URL)) return false;

    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return false;

    String status = doc["status"].as<String>();
    if (status != "success") return false;

    out.lat       = doc["lat"].as<float>();
    out.lon       = doc["lon"].as<float>();
    out.city      = doc["city"].as<String>();
    out.country   = doc["country"].as<String>();
    out.timezone  = doc["timezone"].as<String>();
    out.utcOffset = doc["offset"].as<int>() * 3600; // ip-api gives hours
    out.valid     = true;
    return true;
}

bool WeatherClient::fetchWeather(float lat, float lon,
                                  const String &apiKey, WeatherData &out) {
    if (apiKey.isEmpty()) return false;

    char url[256];
    snprintf(url, sizeof(url),
             "%s?lat=%.5f&lon=%.5f&appid=%s&units=metric",
             OWM_URL, lat, lon, apiKey.c_str());

    WiFiClientSecure secure;
    secure.setInsecure(); // acceptable for weather data; swap for cert bundle in production
    HTTPClient https;
    https.setTimeout(10000);
    if (!https.begin(secure, url)) return false;

    int code = https.GET();
    if (code != 200) { https.end(); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, https.getStream());
    https.end();
    if (err) return false;

    out.tempC       = doc["main"]["temp"].as<float>();
    out.feelsLikeC  = doc["main"]["feels_like"].as<float>();
    out.pressureHPa = doc["main"]["pressure"].as<float>();
    out.humidity    = doc["main"]["humidity"].as<float>();
    out.windSpeedMs = doc["wind"]["speed"].as<float>();
    out.windDeg     = doc["wind"]["deg"]  | 0;
    if (doc["weather"].size() > 0) {
        out.description = doc["weather"][0]["description"].as<String>();
        out.iconCode    = doc["weather"][0]["icon"].as<String>();
    }
    out.valid = true;
    return true;
}
