#pragma once
#include <Arduino.h>

struct GeoInfo {
    float  lat       = 0;
    float  lon       = 0;
    String city;
    String country;
    String timezone;   // e.g. "Europe/Stockholm"
    int    utcOffset  = 0; // seconds east of UTC
    bool   valid      = false;
};

struct WeatherData {
    float  tempC       = 0;
    float  feelsLikeC  = 0;
    float  pressureHPa = 1013;
    float  humidity    = 0;
    float  windSpeedMs = 0;
    int    windDeg     = 0;
    String description;
    String iconCode;
    bool   valid       = false;
};

class WeatherClient {
public:
    // Populate GeoInfo from ip-api.com (no key required).
    bool fetchLocation(GeoInfo &out);

    // Populate WeatherData from OpenWeatherMap current-weather endpoint.
    bool fetchWeather(float lat, float lon, const String &apiKey, WeatherData &out);
};
