#include "AppContext.h"
#include "config.h"

void AppContext::saveGaugePositions() {
    settings.saveGaugeSteps(instruments.getWdirSteps(), instruments.getPresSteps());
}

void AppContext::applyCalMode(bool enable) {
    calMode = enable;
    if (gaugeCal.active()) return;   // don't move gauges while the wizard owns them

    if (enable) {
        instruments.setWindDir(CAL_WDIR_DEG);                 // North
        instruments.setPressure(CAL_PRES_HPA);
        speedMeter.setKnots(msToKnots(CAL_WIND_MS));
    } else if (wx.valid) {
        instruments.setWindDir(wx.windDeg);
        instruments.setPressure(wx.pressureHPa);
        speedMeter.setKnots(msToKnots(wx.windSpeedMs));
    }
    instruments.idle();
    saveGaugePositions();
}

void AppContext::requestCalMode(bool enable) {
    if (enable == calMode) return;
    _calModeRequested  = true;
    _calModeRequestVal = enable;
}

bool AppContext::takePendingCalMode(bool &value) {
    if (!_calModeRequested) return false;
    _calModeRequested = false;
    value = _calModeRequestVal;
    return true;
}

void AppContext::refreshAfterCalib() {
    applyCalMode(calMode);   // re-park or return to weather, depending on calMode
    if (!calMode && wx.valid) slideshow.showWeather(wx, millis());
}

void AppContext::scheduleWeatherRefresh(unsigned long inMs) {
    if (!running) return;   // the state machine will fetch on its own way up
    lastWeatherMs = millis() - WEATHER_INTERVAL_MS + inMs;
}

void AppContext::refreshWeatherBoot() {
    if (!weatherBootPath.isEmpty())
        weatherDisp.showImage(weatherBootPath.c_str());
    else
        weatherDisp.showSplash("v" FW_VERSION, __DATE__);
}
