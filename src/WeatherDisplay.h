#pragma once
#include "BaseDisplay.h"
#include "WeatherClient.h"

class WeatherDisplay : public BaseDisplay {
public:
    // rst=-1 by default: secondary display shares RST with the primary.
    explicit WeatherDisplay(int8_t cs, int8_t rst = -1);

    // Full redraw with current weather data.  Call after each successful fetch.
    void update(const WeatherData &wd);

private:
    void drawWindCompass(int cx, int cy, int r, int deg);
    static const char *degToCompass(int deg);
};
