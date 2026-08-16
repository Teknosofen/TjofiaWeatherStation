#include "Settings.h"
#include "config.h"

Settings::Scope::Scope(Preferences &p, bool readOnly) : _p(p) {
    _p.begin(NVS_NS, readOnly);
}

Settings::Scope::~Scope() {
    _p.end();
}

// ── Gauge step positions ─────────────────────────────────────────────────────

void Settings::loadGaugeSteps(int &wdirSteps, int &presSteps) {
    Scope s(_p, true);
    wdirSteps = _p.getInt(NVS_WIND_STEPS, 0);
    presSteps = _p.getInt(NVS_PRES_STEPS, 0);
}

void Settings::saveGaugeSteps(int wdirSteps, int presSteps) {
    Scope s(_p, false);
    _p.putInt(NVS_WIND_STEPS, wdirSteps);
    _p.putInt(NVS_PRES_STEPS, presSteps);
}

// ── Gauge two-point calibration ──────────────────────────────────────────────

bool Settings::loadGaugeCal(bool wdir, float &zeroSteps, float &gain) {
    Scope s(_p, true);
    zeroSteps = _p.getFloat(wdir ? NVS_WDIR_ZERO : NVS_PRES_ZERO, -1.0f);
    gain      = _p.getFloat(wdir ? NVS_WDIR_GAIN : NVS_PRES_GAIN, -1.0f);
    return gain > 0.0f;
}

void Settings::saveGaugeCal(bool wdir, float zeroSteps, float gain) {
    Scope s(_p, false);
    _p.putFloat(wdir ? NVS_WDIR_ZERO : NVS_PRES_ZERO, zeroSteps);
    _p.putFloat(wdir ? NVS_WDIR_GAIN : NVS_PRES_GAIN, gain);
}

void Settings::clearGaugeCal(bool wdir) {
    Scope s(_p, false);
    _p.remove(wdir ? NVS_WDIR_ZERO : NVS_PRES_ZERO);
    _p.remove(wdir ? NVS_WDIR_GAIN : NVS_PRES_GAIN);
}

// ── Speed meter ──────────────────────────────────────────────────────────────

float Settings::loadSpeedFullScaleMv(float dflt) {
    Scope s(_p, true);
    return _p.getFloat(NVS_PWM_FS_MV, dflt);
}

void Settings::saveSpeedFullScaleMv(float mv) {
    Scope s(_p, false);
    _p.putFloat(NVS_PWM_FS_MV, mv);
}

// ── OpenWeatherMap key ───────────────────────────────────────────────────────

String Settings::loadOwmKey() {
    Scope s(_p, true);
    return _p.getString(NVS_OWM_KEY, "");
}

void Settings::saveOwmKey(const String &key) {
    Scope s(_p, false);
    _p.putString(NVS_OWM_KEY, key);
}

// ── Location ─────────────────────────────────────────────────────────────────

void Settings::loadLocation(float &lat, float &lon, String &tz, int &utcOffset, bool &pinned) {
    Scope s(_p, true);
    lat       = _p.getFloat (NVS_LAT,        0);
    lon       = _p.getFloat (NVS_LON,        0);
    tz        = _p.getString(NVS_TZ,         "");
    utcOffset = _p.getInt   (NVS_UTC_OFF,    0);
    pinned    = _p.getBool  (NVS_LOC_PINNED, false);
}

bool Settings::locationPinned() {
    Scope s(_p, true);
    return _p.getBool(NVS_LOC_PINNED, false);
}

void Settings::saveCoords(float lat, float lon) {
    Scope s(_p, false);
    _p.putFloat(NVS_LAT, lat);
    _p.putFloat(NVS_LON, lon);
}

void Settings::savePinnedCoords(float lat, float lon) {
    Scope s(_p, false);
    _p.putFloat(NVS_LAT, lat);
    _p.putFloat(NVS_LON, lon);
    _p.putBool (NVS_LOC_PINNED, true);
}

void Settings::saveTimezone(const String &tz, int utcOffset) {
    Scope s(_p, false);
    _p.putString(NVS_TZ,      tz);
    _p.putInt   (NVS_UTC_OFF, utcOffset);
}

void Settings::clearLocationPin() {
    Scope s(_p, false);
    _p.remove(NVS_LOC_PINNED);
}

// ── Boot image ───────────────────────────────────────────────────────────────

String Settings::loadBootImage() {
    Scope s(_p, true);
    return _p.getString(NVS_BOOT_IMG, "");
}

void Settings::saveBootImage(const String &path) {
    Scope s(_p, false);
    _p.putString(NVS_BOOT_IMG, path);
}

void Settings::clearBootImage() {
    Scope s(_p, false);
    _p.remove(NVS_BOOT_IMG);
}

// ── Slideshow ────────────────────────────────────────────────────────────────

int Settings::loadSlideSec(int dflt) {
    Scope s(_p, true);
    return _p.getInt(NVS_SLIDE_SEC, dflt);
}

void Settings::saveSlideSec(int sec) {
    Scope s(_p, false);
    _p.putInt(NVS_SLIDE_SEC, sec);
}
