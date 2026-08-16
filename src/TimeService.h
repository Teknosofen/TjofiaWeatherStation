#pragma once
#include <Arduino.h>

// NTP synchronisation and local wall-clock readout.
class TimeService {
public:
    // Configure the timezone from a raw UTC offset and block until NTP replies
    // (up to ~10 s). Returns true when the system clock is valid.
    bool syncNtp(int utcOffsetSec);

    bool ready() const { return _ready; }

    void local(int &h, int &m, int &s) const;

    // "HH:MM:SS", or "--:--:--" before the first successful sync.
    void formatHms(char *buf, size_t n) const;

private:
    bool _ready = false;
};
