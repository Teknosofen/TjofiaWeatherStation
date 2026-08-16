#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Single owner of the NVS namespace.
//
// Every persisted value goes through a named accessor here, so the raw NVS_*
// keys in config.h are referenced from exactly one translation unit and no
// caller has to remember to pair prefs.begin() with prefs.end().
class Settings {
public:
    // ── Gauge step positions ─────────────────────────────────────────────────
    void loadGaugeSteps(int &wdirSteps, int &presSteps);
    void saveGaugeSteps(int wdirSteps, int presSteps);

    // ── Gauge two-point calibration ──────────────────────────────────────────
    // Returns false when the gauge has never been calibrated (gain absent).
    bool loadGaugeCal(bool wdir, float &zeroSteps, float &gain);
    void saveGaugeCal(bool wdir, float zeroSteps, float gain);
    void clearGaugeCal(bool wdir);

    // ── Speed meter ──────────────────────────────────────────────────────────
    float loadSpeedFullScaleMv(float dflt);
    void  saveSpeedFullScaleMv(float mv);

    // ── OpenWeatherMap key ───────────────────────────────────────────────────
    String loadOwmKey();
    void   saveOwmKey(const String &key);

    // ── Location ─────────────────────────────────────────────────────────────
    void loadLocation(float &lat, float &lon, String &tz, int &utcOffset, bool &pinned);
    bool locationPinned();
    void saveCoords(float lat, float lon);          // without touching the pin flag
    void savePinnedCoords(float lat, float lon);    // coords + pin = true
    void saveTimezone(const String &tz, int utcOffset);
    void clearLocationPin();

    // ── Boot image ───────────────────────────────────────────────────────────
    String loadBootImage();
    void   saveBootImage(const String &path);
    void   clearBootImage();

    // ── Slideshow ────────────────────────────────────────────────────────────
    int  loadSlideSec(int dflt);
    void saveSlideSec(int sec);

private:
    Preferences _p;

    // Scoped open/close so no accessor can leak an open NVS handle.
    class Scope {
    public:
        Scope(Preferences &p, bool readOnly);
        ~Scope();
    private:
        Preferences &_p;
    };
};
