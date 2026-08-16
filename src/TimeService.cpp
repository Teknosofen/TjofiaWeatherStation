#include "TimeService.h"
#include "config.h"
#include <time.h>

bool TimeService::syncNtp(int utcOffsetSec) {
    // configTime() on ESP32 generates an invalid POSIX tz string when
    // daylightOffset_sec = 0 (e.g. "UTC-1UTC-11"), which the C library
    // rejects and falls back to UTC.  Build the string ourselves instead.
    //
    // POSIX sign convention is inverted vs common usage:
    //   "UTC-1" = one hour east of UTC  (= UTC+1)
    //   "UTC+5" = five hours west of UTC (= UTC-5)
    int h = utcOffsetSec / 3600;
    int m = abs((utcOffsetSec % 3600) / 60);
    char tzPosix[20];
    if (m == 0)
        snprintf(tzPosix, sizeof(tzPosix), "UTC%+d",      -h);
    else
        snprintf(tzPosix, sizeof(tzPosix), "UTC%+d:%02d", -h, m);
    Serial.printf("NTP sync: offset %+d s → POSIX \"%s\"\n", utcOffsetSec, tzPosix);

    configTzTime(tzPosix, NTP_SERVER1, NTP_SERVER2);
    time_t now = 0;
    for (int i = 0; i < 20 && now < 100000; i++) {
        delay(500);
        now = time(nullptr);
    }
    _ready = (now > 100000);
    return _ready;
}

void TimeService::local(int &h, int &m, int &s) const {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    h = t.tm_hour;
    m = t.tm_min;
    s = t.tm_sec;
}

void TimeService::formatHms(char *buf, size_t n) const {
    if (!_ready) {
        snprintf(buf, n, "--:--:--");
        return;
    }
    int h, m, s;
    local(h, m, s);
    snprintf(buf, n, "%02d:%02d:%02d", h, m, s);
}
