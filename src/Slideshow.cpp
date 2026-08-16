#include "Slideshow.h"

void Slideshow::start(unsigned long now) {
    if (_started) return;
    _started      = true;
    _showingImage = false;
    _lastMs       = now;
}

void Slideshow::showWeather(const WeatherData &wx, unsigned long now) {
    _showingImage = false;
    _lastMs       = now;
    _disp.update(wx);          // shows "No data" when !wx.valid
}

void Slideshow::tick(unsigned long now, const WeatherData &wx) {
    if (!_started) return;
    if (now - _lastMs < (unsigned long)_intervalSec * 1000UL) return;
    _lastMs = now;
    advance(wx);
}

void Slideshow::advance(const WeatherData &wx) {
    if (_showingImage) {
        _showingImage = false;
        _disp.update(wx);
        return;
    }
    // Weather -> next image; wrap when the index runs off the end.
    String path = _store.nth(_nextImage);
    if (path.isEmpty() && _nextImage > 0) {
        _nextImage = 0;
        path = _store.nth(0);
    }
    if (path.isEmpty()) return;   // no images stored — stay on weather
    _disp.showImage(path.c_str());
    _nextImage++;
    _showingImage = true;
}
