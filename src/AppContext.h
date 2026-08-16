#pragma once
#include <Arduino.h>

#include "config.h"
#include "ClockDisplay.h"
#include "WeatherDisplay.h"
#include "Instruments.h"
#include "SpeedMeter.h"
#include "WeatherClient.h"
#include "Settings.h"
#include "ImageStore.h"
#include "Slideshow.h"
#include "GaugeCalWizard.h"
#include "TimeService.h"

// Everything the web layer and the main state machine share.
//
// The modules are constructed in main.cpp and referenced here, so the page
// handlers depend on this one header instead of on a wall of file-scope
// globals. There is exactly one instance: the `app` object below.
struct AppContext {
    // ── Owned modules ────────────────────────────────────────────────────────
    ClockDisplay   clockDisp{TFT_CS};        // primary: owns the shared RST pulse
    WeatherDisplay weatherDisp{TFT2_CS};     // secondary: rst = -1
    Instruments    instruments;
    SpeedMeter     speedMeter{PWM_SPEED_PIN};
    WeatherClient  weather;
    Settings       settings;
    ImageStore     images;
    TimeService    clock;
    Slideshow      slideshow{weatherDisp, images};
    GaugeCalWizard gaugeCal{instruments, settings};

    // ── Live state ───────────────────────────────────────────────────────────
    GeoInfo     geo;
    WeatherData wx;
    String      owmKey;
    String      bootImg;           // NVS-pinned boot image path ("" = none)
    String      weatherBootPath;   // what the weather display shows pre-handover

    bool          calMode       = false;   // needles parked at reference values
    bool          running       = false;   // state machine has reached RUNNING
    unsigned long lastWeatherMs = 0;

    // ── Actions shared between the web layer and loop() ──────────────────────

    // Park the needles at the reference values, or return them to live weather.
    // No-op on the gauges while the calibration wizard holds them.
    void applyCalMode(bool enable);

    // Ask loop() to call applyCalMode(). Web handlers use this so the HTTP
    // response is sent before the motors block the main loop for seconds.
    void requestCalMode(bool enable);
    bool takePendingCalMode(bool &value);

    // Re-apply gauges and hand the weather display back after the wizard exits.
    void refreshAfterCalib();

    void saveGaugePositions();

    // Pull the next weather fetch forward to `inMs` from now.
    void scheduleWeatherRefresh(unsigned long inMs);

    // Re-assert the boot image (or splash) on the weather display. Used between
    // clock-display status messages until live weather takes over.
    void refreshWeatherBoot();

    static float msToKnots(float ms) { return ms * 1.94384f; }

private:
    bool _calModeRequested = false;
    bool _calModeRequestVal = false;
};

extern AppContext app;
