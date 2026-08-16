#pragma once
#include <Arduino.h>
#include "WeatherDisplay.h"
#include "ImageStore.h"
#include "WeatherClient.h"

// Alternates the weather display between live weather and stored photos:
//   weather -> photo 1 -> weather -> photo 2 -> ... wrapping at the last photo.
//
// Does nothing until start() is called (the first successful weather fetch),
// so the boot image stays on screen while the station is still coming up.
class Slideshow {
public:
    static constexpr int DEFAULT_SEC = 10;
    static constexpr int MIN_SEC     = 1;
    static constexpr int MAX_SEC     = 300;

    Slideshow(WeatherDisplay &disp, ImageStore &store) : _disp(disp), _store(store) {}

    void setIntervalSec(int sec) { _intervalSec = sec; }
    int  intervalSec() const     { return _intervalSec; }

    bool started() const { return _started; }

    // Begin cycling from the weather slide. Idempotent.
    void start(unsigned long now);

    // Restart the dwell timer without changing which slide is showing.
    void restartTimer(unsigned long now) { _lastMs = now; }

    // Force the weather slide on screen and restart the dwell timer.
    // Called whenever fresh data arrives or the calibration UI hands control back.
    void showWeather(const WeatherData &wx, unsigned long now);

    // Call from loop(); advances at most one slide per interval.
    void tick(unsigned long now, const WeatherData &wx);

private:
    WeatherDisplay &_disp;
    ImageStore     &_store;

    int           _intervalSec = DEFAULT_SEC;
    bool          _started     = false;
    bool          _showingImage = false;
    int           _nextImage   = 0;
    unsigned long _lastMs      = 0;

    void advance(const WeatherData &wx);
};
