#include "WebUI.h"
#include "WebPages.h"
#include "AppContext.h"
#include "config.h"

// /calib — three separate calibrations share this page:
//   1. Reference position   — park both needles at known values (checkbox)
//   2. Gauge calibration    — two-point wizard, one gauge at a time
//   3. Wind speed meter     — full-scale PWM voltage

namespace {

const char CSS[] PROGMEM =
    "body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 12px}"
    "h1{font-size:1.2em}h2{font-size:1em;margin:0 0 6px}"
    "input[type=number]{padding:8px;border:1px solid #ccc;border-radius:4px;"
                       "box-sizing:border-box}"
    ".btn{display:inline-block;padding:8px 14px;background:#1fa3ec;color:#fff;"
        "border:none;border-radius:4px;cursor:pointer;text-decoration:none;margin:2px}"
    ".nbtn{font-size:1.1em;padding:11px 18px;min-width:58px}"
    ".red{background:#c00}.grn{background:#2a2}"
    ".box{background:#f9f9f9;border:1px solid #ddd;border-radius:6px;"
         "padding:14px;margin:14px 0}"
    "label{cursor:pointer}"
    "input[type=checkbox]{width:18px;height:18px;vertical-align:middle;margin-right:6px}"
    "small,span.dim{color:#666;font-size:.85em}"
    ".gcrow{display:flex;align-items:center;gap:8px;"
           "padding:8px 0;border-top:1px solid #eee}"
    ".gcrow:first-of-type{border-top:none}"
    ".gcrow .lbl{flex:1}";

constexpr float SPEED_FS_MIN_MV = 50.0f;
constexpr float SPEED_FS_MAX_MV = 3300.0f;

// One row of the idle wizard panel: current coefficients + Calibrate / Reset.
void appendGaugeRow(String &p, const char *title, bool wdir) {
    float zero, gain;
    char  buf[80];
    bool  calibrated;
    if (wdir) { app.instruments.getWdirCalibration(zero, gain); calibrated = app.instruments.wdirCalibrated(); }
    else      { app.instruments.getPresCalibration(zero, gain); calibrated = app.instruments.presCalibrated(); }
    const char *unit = wdir ? "&deg;" : "hPa";
    const char *tag  = wdir ? "wdir"  : "pres";

    p += F("<div class='gcrow'><div class='lbl'><b>");
    p += title;
    p += F("</b><br><span class='dim'>");
    if (calibrated) snprintf(buf, sizeof(buf), "zero %.0f steps, %.3f steps/%s", zero, gain, unit);
    else            snprintf(buf, sizeof(buf), "factory default (%.2f steps/%s)", gain, unit);
    p += buf;
    p += F("</span></div><div>"
           "<form method='POST' action='/calib/gc-start' style='display:inline'>"
           "<input type='hidden' name='gauge' value='"); p += tag;
    p += F("'><button class='btn' type='submit'>Calibrate</button></form>");
    if (calibrated) {
        p += F(" <form method='POST' action='/calib/gc-reset' style='display:inline'>"
               "<input type='hidden' name='gauge' value='"); p += tag;
        p += F("'><button class='btn red' type='submit'>Reset</button></form>");
    }
    p += F("</div></div>");
}

void appendWizardIdle(String &p) {
    p += F("<div class='box'><h2>Gauge calibration</h2>"
           "<p><small>Two-point linear calibration: nudge the needle to two known marks, "
           "confirm each &mdash; offset and scale are computed automatically.</small></p>");
    appendGaugeRow(p, "Wind direction",      true);
    appendGaugeRow(p, "Barometric pressure", false);
    p += F("</div>");
}

void appendWizardActive(String &p) {
    GaugeCalWizard &wiz = app.gaugeCal;
    const bool  isWdir    = wiz.isWdir();
    const char *gaugeName = isWdir ? "Wind direction" : "Barometric pressure";
    const char *units     = isWdir ? "&deg;"          : " hPa";
    const int   curPos    = wiz.position();
    const int   ptNum     = wiz.pointNumber();
    char        buf[64];

    p += F("<div class='box'><h2>Calibrating: ");
    p += gaugeName;
    p += F(" &mdash; point ");
    p += (ptNum == 1 ? "1" : "2");
    p += F(" of 2</h2>"
           "<p><small>Nudge the needle to a printed mark on the scale, enter the "
           "value it points to, then confirm.</small></p>"
           "<p>Position: <b id='gcpos'>");
    p += curPos;
    p += F("</b> steps</p>"
           "<div style='display:flex;flex-wrap:wrap;gap:6px;margin:10px 0'>"
           "<button class='btn nbtn' onclick='nudge(-1000)'>&#8722;1000</button>"
           "<button class='btn nbtn' onclick='nudge(-100)'>&#8722;100</button>"
           "<button class='btn nbtn' onclick='nudge(-10)'>&#8722;10</button>"
           "<button class='btn nbtn' onclick='nudge(-1)'>&#8722;1</button>"
           "<button class='btn nbtn' onclick='nudge(1)'>+1</button>"
           "<button class='btn nbtn' onclick='nudge(10)'>+10</button>"
           "<button class='btn nbtn' onclick='nudge(100)'>+100</button>"
           "<button class='btn nbtn' onclick='nudge(1000)'>+1000</button>"
           "<button class='btn nbtn' onclick='home()' style='background:#555'>Home</button>"
           "</div>");

    if (ptNum == 2) {
        snprintf(buf, sizeof(buf), "%.4g", wiz.p1Value());
        p += F("<p><span class='dim'>Point 1 confirmed: ");
        p += buf; p += units;
        p += F(" at step "); p += wiz.p1Steps();
        p += F("</span></p>");
    }

    p += F("<form method='POST' action='/calib/gc-confirm' style='margin-top:10px'>"
           "<label>Scale value at this needle position (");
    p += units;
    p += F("): <input type='number' name='val' required step='0.1' min='");
    snprintf(buf, sizeof(buf), "%.1f", wiz.minValue());
    p += buf;
    p += F("' max='");
    snprintf(buf, sizeof(buf), "%.1f", wiz.maxValue());
    p += buf;
    p += F("' style='width:110px;margin-left:6px'></label> "
           "<button class='btn grn' type='submit'>Confirm point ");
    p += (ptNum == 1 ? "1" : "2");
    p += F("</button></form>"
           "<form method='POST' action='/calib/gc-cancel' style='margin-top:8px'>"
           "<button class='btn red' type='submit'>Cancel</button></form>");

    // Nudge over AJAX so a button press does not reload the whole page.
    p += F("<script>"
           "var gcPos=");
    p += curPos;
    p += F(",gcGauge='");
    p += (isWdir ? "wdir" : "pres");
    p += F("';"
           "function nudge(n){"
           "document.querySelectorAll('.nbtn').forEach(function(b){b.disabled=true;});"
           "fetch('/calib/nudge',{method:'POST',"
           "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
           "body:'gauge='+gcGauge+'&steps='+n})"
           ".then(function(r){return r.json();})"
           ".then(function(d){"
           "gcPos=d.pos;"
           "document.getElementById('gcpos').textContent=gcPos;"
           "document.querySelectorAll('.nbtn').forEach(function(b){b.disabled=false;});"
           "}).catch(function(){"
           "document.querySelectorAll('.nbtn').forEach(function(b){b.disabled=false;});});"
           "}"
           "function home(){nudge(-gcPos);}"
           "</script>");

    p += F("</div>");
}

String buildPage() {
    const bool wizActive = app.gaugeCal.active();
    char fsMvStr[12], curMvStr[12];
    snprintf(fsMvStr,  sizeof(fsMvStr),  "%.0f", app.speedMeter.getFullScaleMv());
    snprintf(curMvStr, sizeof(curMvStr), "%.0f", app.speedMeter.getCurrentMv());

    String p;
    p.reserve(5000);
    WebUI::pageHead(p, F("Tjofia WX &mdash; Calibration"), FPSTR(CSS));
    p += F("<h1>&#9881; Calibration</h1>");

    // ── Reference position ───────────────────────────────────────────────────
    p += F("<div class='box'><h2>Stepper reference position</h2>"
           "<form method='POST' action='/cal'>"
           "<label><input type='checkbox' name='cal'");
    if (app.calMode) p += F(" checked");
    if (wizActive)   p += F(" disabled");
    p += F(" onchange='this.form.submit()'>"
           "Move dials to reference &mdash; North (0&deg;) &bull; 1000&nbsp;hPa"
           "</label></form>");
    if (wizActive)
        p += F("<p><small>Disabled while gauge calibration wizard is active.</small></p>");
    else
        p += F("<p><small>Position saved to NVS. If power is cycled while active, "
               "motors resume at the reference position.</small></p>");
    p += F("</div>");

    // ── Gauge calibration wizard ─────────────────────────────────────────────
    if (wizActive) appendWizardActive(p);
    else           appendWizardIdle(p);

    // ── Wind speed meter ─────────────────────────────────────────────────────
    p += F("<div class='box'>"
           "<h2>Wind speed meter (PWM &rarr; GPIO22)</h2>"
           "<p><small>Range: 0&ndash;30 kn. Set the output voltage (mV) that drives"
           " the needle to full-scale. Current output: ");
    p += curMvStr;
    p += F(" mV.</small></p>"
           "<form method='POST' action='/calib/save'>"
           "<label>Full-scale voltage (mV) "
           "<input type='number' name='fs_mv' min='50' max='3300' step='1' value='");
    p += fsMvStr;
    p += F("' style='width:90px'></label> "
           "<button class='btn' type='submit'>Save</button>"
           "</form></div>");

    // While the wizard is active the Back button must also cancel it, otherwise
    // the gauges stay frozen and weather updates never resume.
    if (wizActive)
        p += F("<form method='POST' action='/calib/gc-back'>"
               "<button class='btn' type='submit'>&#8592; Back</button></form>");
    else
        p += F("<a class='btn' href='/'>&#8592; Back</a>");
    p += F("</body></html>");
    return p;
}

void handlePage() {
    WebUI::server.send(200, "text/html", buildPage());
}

void handleGcStart() {
    app.gaugeCal.start(WebUI::server.arg("gauge") != "pres");
    WebUI::redirect(F("/calib"));
}

void handleGcNudge() {
    if (!app.gaugeCal.active()) {
        WebUI::server.send(400, "application/json", F("{\"err\":\"idle\"}"));
        return;
    }
    int pos = app.gaugeCal.nudge(WebUI::server.arg("steps").toInt());
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"pos\":%d}", pos);
    WebUI::server.send(200, "application/json", buf);
}

void handleGcConfirm() {
    if (app.gaugeCal.active() && app.gaugeCal.confirm(WebUI::server.arg("val").toFloat())) {
        app.saveGaugePositions();
        app.refreshAfterCalib();
    }
    WebUI::redirect(F("/calib"));
}

void handleGcCancel() {
    app.gaugeCal.cancel();
    app.refreshAfterCalib();
    WebUI::redirect(F("/calib"));
}

// Cancel the wizard and navigate home in one step.
void handleGcBack() {
    app.gaugeCal.cancel();
    app.refreshAfterCalib();
    WebUI::redirect(F("/"));
}

void handleGcReset() {
    app.gaugeCal.resetGauge(WebUI::server.arg("gauge") != "pres");
    WebUI::redirect(F("/calib"));
}

void handleSpeedSave() {
    if (WebUI::server.hasArg("fs_mv")) {
        float mv = WebUI::server.arg("fs_mv").toFloat();
        if (mv >= SPEED_FS_MIN_MV && mv <= SPEED_FS_MAX_MV) {
            app.speedMeter.setFullScaleMv(mv);
            // Re-apply the current speed so the needle reflects the new scale at once.
            if (app.calMode)
                app.speedMeter.setKnots(AppContext::msToKnots(CAL_WIND_MS));
            else if (app.wx.valid)
                app.speedMeter.setKnots(AppContext::msToKnots(app.wx.windSpeedMs));
            app.settings.saveSpeedFullScaleMv(mv);
            Serial.printf("Speed meter full-scale saved: %.0f mV\n", mv);
        }
    }
    WebUI::redirect(F("/calib"));
}

}  // namespace

void WebPages::registerCalib(WebServer &s) {
    s.on("/calib",             HTTP_GET,  handlePage);
    s.on("/calib/save",        HTTP_POST, handleSpeedSave);
    s.on("/calib/gc-start",    HTTP_POST, handleGcStart);
    s.on("/calib/nudge",       HTTP_POST, handleGcNudge);
    s.on("/calib/gc-confirm",  HTTP_POST, handleGcConfirm);
    s.on("/calib/gc-cancel",   HTTP_POST, handleGcCancel);
    s.on("/calib/gc-back",     HTTP_POST, handleGcBack);
    s.on("/calib/gc-reset",    HTTP_POST, handleGcReset);
}
