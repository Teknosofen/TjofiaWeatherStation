# 28BYJ-48 Dual Stepper Motor Control
**ULN2003A Driver · Adafruit HUZZAH32 (ESP32-WROOM) · GC9A01 SPI Display**

---

## 1. Overview

This guide covers control of two 28BYJ-48 stepper motors from an Adafruit HUZZAH32
(ESP32-WROOM32 Feather) using two separate ULN2003A driver chips. A circular SPI
display (GC9A01, 240×240) shows weather and clock data. All control signals run at
3.3V logic; the motor coils are powered from a separate 5V supply.

| Parameter | Value |
|---|---|
| Motor | 28BYJ-48, 5V unipolar stepper |
| Driver (×2) | ULN2003A, DIP-16 |
| Controller | Adafruit HUZZAH32 (ESP32-WROOM32) |
| Display | GC9A01 240×240 circular, hardware SPI (VSPI) |
| Display library | DIYables_TFT_Round (Adafruit GFX compatible) |
| Wind speed meter | Analog panel meter, 0–30 kn, driven by 12-bit LEDC PWM (GPIO22) |
| Steps / rev (half-step) | 4096 — output shaft, internal 1:64 gear included |
| Gear ratio | 1:64 internal (motor only — no external reduction) |
| Motor supply | 5V external — NOT the 3.3V board rail |

---

## 2. HUZZAH32 Pin Allocation

The HUZZAH32 uses an ESP32-WROOM32 module. Some board pins carry alternate labels
(A0–A5) that do not match the underlying GPIO number — the table below shows both.

| Board label | GPIO # | Assigned to | Notes |
|---|---|---|---|
| SCK | 5 | Display CLK | SPI SCK — shared by both displays |
| MOSI | 18 | Display MOSI | SPI MOSI — shared by both displays |
| D15 | 15 | Display 1 CS | Clock face display — chip select |
| D13 | 13 | Display DC | Data/command — shared by both displays |
| A5 | 4 | Display RST | Reset — shared by both displays (wired together) |
| 21 | 21 | Display 2 CS | Weather display — chip select (`TFT2_CS`) |
| 22 | 22 | PWM speed out | Electrical speed indicator output (`PWM_SPEED_PIN`) |
| 14 | 14 | Motor 1 — IN1 | Wind gauge |
| 27 | 27 | Motor 1 — IN2 | Wind gauge |
| 32 | 32 | Motor 1 — IN3 | Wind gauge |
| 33 | 33 | Motor 1 — IN4 | Wind gauge |
| A1 | 25 | Motor 2 — IN1 | Pressure gauge — board label A1, actual GPIO 25 |
| A0 | 26 | Motor 2 — IN2 | Pressure gauge — board label A0, actual GPIO 26 |
| RX | 16 | Motor 2 — IN3 | Pressure gauge — safe if Serial1 unused |
| TX | 17 | Motor 2 — IN4 | Pressure gauge — safe if Serial1 unused |
| 19 | 19 | (free) | SPI MISO — not needed (write-only displays); reserve for future use |
| 34, 35, 36, 39 | — | Do not use for output | Input-only pins — no output driver |
| 6–11 | — | Do not use | Internal SPI flash — never expose |

> **Note:** The A0/A1 labels on the HUZZAH32 silk refer to GPIO 26 and 25
> respectively — not GPIO 0 and 1. Always use the GPIO number in code, not the
> A-prefix label.

> **Note:** RX (GPIO 16) and TX (GPIO 17) are usable as general outputs as long
> as you do not need the hardware Serial1 port.

### 2.1 Second display wiring

Both GC9A01 displays share the same SPI bus (SCK, MOSI) and the same DC and RST
lines. Only the CS pin differs:

| Signal | Both displays | Display 1 only | Display 2 only |
|--------|--------------|----------------|----------------|
| CLK | GPIO 5 | — | — |
| MOSI | GPIO 18 | — | — |
| DC | GPIO 13 | — | — |
| RST | GPIO 4 | — | — |
| CS | — | GPIO 15 | GPIO 21 |

Wire the second display's CLK, MOSI, DC, and RST to the **same** board pins as
the first display. Connect only CS to GPIO 21.

**Initialisation order matters:** `clockDisp.begin()` is called first — this drives
the shared RST line and resets both displays simultaneously. `weatherDisp.begin()`
is called second with `rst=-1` so it sends init commands to the second display
without toggling RST again (which would reset the already-running first display).

---

## 3. Motor Connector — Wire Colours

The 28BYJ-48 uses a 5-pin JST PH (2 mm pitch) connector. Pin numbering is with the
latch/tab facing you, left to right. Both motors use identical connectors and wire
colours.

| Pin | Colour | Coil | ULN OUT pin | Connect to |
|---|---|---|---|---|
| 1 | Blue | B2 (IN4) | Pin 13 | Motor 1: GPIO 33 / Motor 2: GPIO 17 (TX) |
| 2 | Pink | B1 (IN3) | Pin 14 | Motor 1: GPIO 32 / Motor 2: GPIO 16 (RX) |
| 3 | Yellow | A2 (IN2) | Pin 15 | Motor 1: GPIO 27 / Motor 2: GPIO 26 (A0) |
| 4 | Orange | A1 (IN1) | Pin 16 | Motor 1: GPIO 14 / Motor 2: GPIO 25 (A1) |
| 5 | Red | +5V common | — | +5V supply + ULN2003A pin 9 (COM) |

> **Note:** The red wire must connect to both +5V and the ULN2003A COM pin (pin 9).
> This activates the internal flyback diodes which suppress voltage spikes when
> coils switch off.

---

## 4. ULN2003A Chip Pinout (DIP-16)

Wire both ULN2003A chips identically — only the GPIO source pins differ between
Motor 1 and Motor 2.

| Pin | Signal | Pin | Signal | Notes |
|---|---|---|---|---|
| 1 | IN1 ← GPIO (see §2) | 16 | OUT1 → Orange wire | Coil A1 |
| 2 | IN2 ← GPIO (see §2) | 15 | OUT2 → Yellow wire | Coil A2 |
| 3 | IN3 ← GPIO (see §2) | 14 | OUT3 → Pink wire | Coil B1 |
| 4 | IN4 ← GPIO (see §2) | 13 | OUT4 → Blue wire | Coil B2 |
| 5 | IN5 (unused) | 12 | OUT5 (unused) | |
| 6 | IN6 (unused) | 11 | OUT6 (unused) | |
| 7 | IN7 (unused) | 10 | OUT7 (unused) | |
| 8 | GND | 9 | COM → +5V | Must connect to motor supply |

---

## 5. Full Wiring Summary

**Motor 1 (Wind gauge)**

| HUZZAH32 label | GPIO # | ULN2003A #1 | Motor 1 wire |
|---|---|---|---|
| 14 | 14 | Pin 1 (IN1) → Pin 16 (OUT1) | Orange |
| 27 | 27 | Pin 2 (IN2) → Pin 15 (OUT2) | Yellow |
| 32 | 32 | Pin 3 (IN3) → Pin 14 (OUT3) | Pink |
| 33 | 33 | Pin 4 (IN4) → Pin 13 (OUT4) | Blue |
| +5V | — | Pin 9 (COM) | Red (common) |
| GND | — | Pin 8 (GND) | — |

**Motor 2 (Pressure gauge)**

| HUZZAH32 label | GPIO # | ULN2003A #2 | Motor 2 wire |
|---|---|---|---|
| A1 | 25 | Pin 1 (IN1) → Pin 16 (OUT1) | Orange |
| A0 | 26 | Pin 2 (IN2) → Pin 15 (OUT2) | Yellow |
| RX | 16 | Pin 3 (IN3) → Pin 14 (OUT3) | Pink |
| TX | 17 | Pin 4 (IN4) → Pin 13 (OUT4) | Blue |
| +5V | — | Pin 9 (COM) | Red (common) |
| GND | — | Pin 8 (GND) | — |

**GC9A01 SPI Displays — shared signals (wire to both displays)**

| HUZZAH32 label | GPIO # | Display pin label | Notes |
|---|---|---|---|
| SCK | 5 | **SCK** | SPI clock — board label matches display label |
| MOSI | 18 | **SDA** | SPI data — display PCB labels this SDA, **not** I2C |
| D13 | 13 | DC | Data / command select |
| A5 | 4 | RST | Reset — wire both display RST pins together to this pin |
| 3V | — | VCC | 3.3V supply — both displays |
| GND | — | GND | Common ground — both displays |

> **⚠ SPI, not I2C:** The GC9A01 module labels its data pin **SDA** and its clock
> pin **SCK**, which makes it look like an I2C device. It is not — it is 4-wire SPI.
> Connect SDA → MOSI (GPIO 18) and SCK → SCK (GPIO 5). There is no I2C address,
> no pull-up resistor, and no SDA/SCL bidirectional bus involved.

**GC9A01 SPI Displays — per-display CS (one wire each)**

| HUZZAH32 label | GPIO # | Connect to | Display |
|---|---|---|---|
| D15 | 15 | CS | Display 1 — clock face |
| 21 | 21 | CS | Display 2 — weather panel |

---

## 6. Display Controller — GC9A01
round display information: https://diyables.io/products/1.28-inch-round-circular-tft-lcd-display-module

The GC9A01 is a single-chip LCD/TFT controller designed for circular or square
displays up to 240×240 pixels. It communicates over 4-wire SPI and operates entirely
at 3.3V — no level shifting is needed when driven from the HUZZAH32.

| Parameter | Value |
|---|---|
| Resolution | 240 × 240 px |
| Colour depth | 16-bit RGB565 |
| Interface | 4-wire SPI (CLK, MOSI, CS, DC) + RST |
| Supply voltage | 3.3V (logic and panel) |
| SPI clock used | 80 MHz (ESP32 VSPI maximum; GC9A01 supports up to 100 MHz) |
| Driver IC | Sitronix GC9A01 |

### 6.1 SPI Signal Roles

| Display pin label | SPI signal | Direction | Function |
|---|---|---|---|
| SCK | CLK | ESP32 → display | SPI clock |
| **SDA** | **MOSI** | ESP32 → display | Serial data in — **not I2C**; display has no MISO |
| CS | CS | ESP32 → display | Chip select — active LOW |
| DC | DC | ESP32 → display | HIGH = data, LOW = command |
| RST | RST | ESP32 → display | Hardware reset — active LOW pulse on startup |
| VCC | — | — | 3.3V supply |
| GND | — | — | Common ground |

> **Pin labelling quirk:** The physical module silkscreen reads **SDA** for the data
> line and **SCK** for the clock, matching I2C conventions. The GC9A01 is purely SPI;
> the labels are a common misnomer on round display breakouts. There is no I2C mode.

### 6.2 Initialisation Sequence

The DIYables_TFT_Round library handles the full GC9A01 init sequence (sleep-out,
colour format, display-on, etc.) inside `begin()`. No manual register writes are
needed.

```cpp
#include <DIYables_TFT_Round.h>

// Pins: RST=4, DC=13, CS=15  (CLK=18 and MOSI=23 are hardware-fixed VSPI)
DIYables_TFT_GC9A01_Round tft(4, 13, 15);

void setup() {
    tft.begin();
    tft.fillScreen(BLACK);
}
```

### 6.3 Colour Format

All colours are 16-bit **RGB565** — 5 bits red, 6 bits green, 5 bits blue. Common
values used in this project:

| Name | Hex | Use |
|---|---|---|
| `COL_BG` | `0x0000` | Background — black |
| `COL_FACE` | `0xFFFF` | Clock face and text — white |
| `COL_ACCENT` | `0xFD20` | Temperature, centre dot — amber |
| `COL_SEC` | `0xF800` | Second hand — red |
| `COL_ERROR` | `0xF800` | Error screen border — red |

---

## 7. WiFi Setup and Weather Service

### 7.1 Weather data source

Weather data comes from **OpenWeatherMap** (OWM), free tier.
The free plan allows 1 000 API calls per day; the firmware fetches every
10 minutes (~144 calls/day), well within the limit.

**Getting an API key:**

1. Create a free account at [openweathermap.org](https://openweathermap.org)
2. Confirm your e-mail address
3. Go to **My Profile → API keys**
4. Copy the default key (or create a named key, e.g. *TjofiaWX*)
5. The key becomes active within a few minutes of account creation

### 7.2 Web portal — access methods

The firmware runs a permanent web portal. Two ways to reach it:

| Situation | How to connect | URL |
|---|---|---|
| Device AP only (first boot, no WiFi yet) | Connect phone/laptop to **TjofiaWX-Setup** | `http://192.168.4.1` |
| Device on home WiFi (normal operation) | Stay on home WiFi | `http://TjofiaWX.local` |
| Device AP always runs in parallel | Connect to **TjofiaWX-Setup** | `http://192.168.4.1` |

> **Note:** `TjofiaWX.local` uses mDNS (Bonjour). It works natively on
> macOS, iOS, and Windows 10 1903+. Android support varies; use the IP
> address `192.168.4.1` via the AP if `.local` does not resolve.

**Portal pages:**

| Path | Purpose |
|---|---|
| `/` | Settings — OWM API key form, current WiFi/IP info, WiFi reset |
| `/wx` | Status — live weather, time, location, gauge steps, speed meter, calibration toggle |
| `/location` | Map-based location pin — set lat/lon manually |
| `/calib` | Calibration — set speed meter full-scale voltage (more options coming) |
| `/reset` | Clears saved WiFi credentials and restarts (confirmation prompt) |

**First boot (no saved WiFi credentials):**

1. Power on the ESP32
2. Hotspot **TjofiaWX-Setup** appears — connect from any phone or laptop
3. Open `http://192.168.4.1` — a WiFiManager credential page loads
4. Select your home WiFi network, enter the password, and paste the OWM API key
5. Save — the device connects to your router

**After first boot:**

The AP **TjofiaWX-Setup** stays visible at all times alongside the home WiFi
connection. The OWM key can be updated at any time via `http://TjofiaWX.local/`
or `http://192.168.4.1/` without reflashing.

### 7.3 Geolocation

**Automatic (IP-based)**

On every boot the firmware calls **ip-api.com** (no key required) to determine
latitude, longitude, timezone, and UTC offset from the device's public IP
address. The request uses an explicit `fields` parameter so the UTC offset is
included (it is not in the default response):

```
http://ip-api.com/json?fields=status,city,country,lat,lon,timezone,offset
```

IP geolocation resolves to the ISP's gateway or exchange, which can be tens of
kilometres from the actual device. For weather data this is usually acceptable,
but if the nearest OWM station is wrong, use the manual pin below.

All values are cached in NVS (`lat`, `lon`, `timezone`, `utc_off`) so they
survive reboots and temporary network outages.

**Manual location pin**

A map-based location picker is available at `/location`
(`http://TjofiaWX.local/location` or a button on the main config page).

> **Requires internet access on the browser** — the ESP32 serves only a small
> HTML skeleton; the Leaflet.js library and OpenStreetMap tiles are fetched by
> the browser from public CDNs. Use the `TjofiaWX.local` address on the home
> WiFi network, not the device AP.

| Step | Action |
|---|---|
| 1 | Open `http://TjofiaWX.local/location` |
| 2 | The map opens centred on the current (IP-geolocated) position with a draggable pin |
| 3 | Click anywhere on the map to move the pin, or drag the marker to fine-tune |
| 4 | Tap **Use my GPS** to jump the map and pin to the browser's GPS position (works best on a phone) |
| 5 | Press **Save & pin** — lat/lon are written to NVS and a `loc_pinned` flag is set |

Once pinned:
- The `loc_pinned` flag is shown with a green banner on the location page.
- ip-api.com is still called on each boot **for the UTC offset only** — its
  lat/lon is silently ignored. Local time therefore remains correct.
- The serial monitor prints `[pinned]` next to the coordinates to confirm.

```
Location: [pinned] (60.1234, 16.5678)  tz offset +7200 s (UTC offset from ip-api)
```

To revert to automatic IP geolocation, press **Clear pin** on the location
page. NVS keys `lat` and `lon` are not deleted — they are simply unprotected
again and will be overwritten by the next successful ip-api fetch.

### 7.4 NTP time sync

Local time is obtained from `pool.ntp.org` / `time.nist.gov` after WiFi
connects. The UTC offset comes from the ip-api geolocation response — no
manual timezone configuration is needed.

The ESP32 Arduino `configTime()` function builds an invalid POSIX timezone
string when the DST offset is zero (e.g. `"UTC-1UTC-11"`), which the C
runtime rejects and silently treats as UTC. The firmware avoids this by
calling `configTzTime()` directly with a manually built POSIX string:

| ip-api `offset` | POSIX string | Meaning |
|---|---|---|
| `+3600` (UTC+1) | `"UTC-1"` | 1 hour east of UTC |
| `+7200` (UTC+2) | `"UTC-2"` | 2 hours east of UTC |
| `-18000` (UTC−5) | `"UTC+5"` | 5 hours west of UTC |

Note the inverted sign — this is the POSIX timezone convention.
Half-hour and quarter-hour offsets (e.g. `+19800` for UTC+5:30) are
handled correctly. The serial monitor prints the offset and the resulting
POSIX string on every boot for easy verification:

```
Location: Råby, Sweden (60.1000, 16.3667)  tz offset +7200 s
NTP sync: offset +7200 s → POSIX "UTC-2"
```

---

## 8. Half-Step Sequence

The same 8-phase sequence applies to both motors. Each row shows the coil states
driven by IN1–IN4.

| Step | IN1 (Orange) | IN2 (Yellow) | IN3 (Pink) | IN4 (Blue) |
|---|---|---|---|---|
| 0 | 1 | 0 | 0 | 0 |
| 1 | 1 | 1 | 0 | 0 |
| 2 | 0 | 1 | 0 | 0 |
| 3 | 0 | 1 | 1 | 0 |
| 4 | 0 | 0 | 1 | 0 |
| 5 | 0 | 0 | 1 | 1 |
| 6 | 0 | 0 | 0 | 1 |
| 7 | 1 | 0 | 0 | 1 |

---

## 9. Project Code Structure

The firmware is a PlatformIO project targeting the `featheresp32` board. Key source
files:

| File | Purpose |
|---|---|
| `src/Stepper28BYJ.h` | Low-level half-step driver — one instance per motor |
| `src/Instruments.h/cpp` | `StepperGauge` and `Instruments` — position-aware gauge abstraction |
| `src/config.h` | All pin assignments, gauge limits, timing constants |
| `src/BaseDisplay.h/cpp` | Abstract base — owns `_tft`, bezel, boot screens (splash/status/error/AP) |
| `src/ClockDisplay.h/cpp` | Extends `BaseDisplay` — analogue clock face, hands, centre temperature |
| `src/WeatherDisplay.h/cpp` | Extends `BaseDisplay` — weather data panel (temp, wind, pressure, humidity) |
| `src/DisplayManager.h` | Thin alias: `typedef ClockDisplay DisplayManager` (kept for compatibility) |
| `src/SpeedMeter.h/cpp` | 12-bit LEDC PWM driver for the analog wind-speed meter panel |
| `src/WeatherClient.h/cpp` | IP geolocation + OpenWeatherMap fetch + Nominatim reverse geocoding |
| `src/main.cpp` | State-machine entry point |
| `src/demo/main.cpp` | Motor test sketch (see §11) |

**Stepper28BYJ** is self-contained — no external stepper library needed.
**StepperGauge** wraps it with a value-to-steps mapping and tracks the current
needle position so only the delta is driven on each update.

### 9.1 Display driver and dual-display architecture

The upstream **DIYables_TFT_Round** library is vendored into `lib/DIYables_TFT_Round/`
with two patches applied:

| Patch | Reason |
|---|---|
| SPI clock raised from 40 MHz → **80 MHz** | Upstream hardcoded 40 MHz; GC9A01 and ESP32 VSPI both support 80 MHz |
| `fillScreen()` rewritten to send **512-byte chunks** | Upstream sent one byte at a time — 115 200 individual transfers to clear the screen; patched version uses 225 bulk writes (~10× faster) |
| `spiTx()` switched to `SPI.writeBytes()` | `SPI.transfer()` overwrites the data buffer (full-duplex); `writeBytes()` is write-only and correct for a display |
| `begin()` skips reset when `_res == 0xFF` | The library stores the reset pin as `uint8_t`; passing `rst=-1` (the "no reset" sentinel) silently becomes `255`. Without this guard, `begin()` called `pinMode(255)` and `digitalWrite(255)`, generating HAL errors. Secondary displays sharing a RST line now pass `rst=-1` cleanly. |

**Class hierarchy**

```
BaseDisplay          owns _tft, drawBezel(), showSplash/Status/Error/APMode
   ├── ClockDisplay  analogue clock face, hour/minute/second hands, centre temp
   └── WeatherDisplay  weather data panel updated after each 10-min fetch
```

Two instances live in `main.cpp`:

```cpp
static ClockDisplay   clockDisp(TFT_CS);      // CS=15, RST=4 (drives shared RST)
static WeatherDisplay weatherDisp(TFT2_CS);   // CS=21, rst=-1 (no RST toggle)
```

**Shared RST initialisation order:**
`clockDisp.begin()` is called first — it drives GPIO 4 (RST) low→high, which resets
both displays simultaneously (RST lines are wired together). `weatherDisp.begin()`
is called second with `rst=-1`; it sends init commands to the second display without
toggling RST again, which would otherwise reset the already-initialised first display.

**WeatherDisplay layout (240×240 circle)**

| y (baseline) | Content | Font | Colour |
|---|---|---|---|
| 72 | Temperature `22.5°C` | FreeSansBold18pt | amber |
| 92 | `feels 20.1°C` | FreeSans9pt | dim white |
| 101 | separator line | — | dark grey |
| 135 | Compass rose (cx=58, cy=135, r=30) + wind speed m/s + bearing | FreeSans9pt | white / amber |
| 178 | Pressure `1013 mBar` | FreeSans9pt | white |
| 193 | Humidity `65% RH` | FreeSans9pt | white |
| 202 | separator line | — | dark grey |
| 215 | Weather description | FreeSans9pt | amber |

The compass needle points FROM the wind source (meteorological convention: `deg=0`
= from North, needle tip at top of circle). Cardinal points N/S/E/W are labelled
inside the ring using the built-in 6×8 font; **N is red**, S/E/W are white.
`WeatherDisplay::update()` does a full `fillScreen` + redraw on each call; at
10-minute intervals the brief black flash is imperceptible.

`platformio.ini` references Adafruit GFX directly (was previously a transitive
dependency of the DIYables library):

```ini
lib_deps =
    adafruit/Adafruit GFX Library @ ^1.11.0
```

> **Note:** The HUZZAH32 routes its SPI bus to SCK=GPIO5 and MOSI=GPIO18 — these
> differ from the raw ESP32 VSPI defaults (CLK=18, MOSI=23). The library picks up
> the correct pins automatically via the board's `SPI.begin()`. Only RST, DC, and
> CS are free to assign.

### 9.2 Gauge physical limits (`config.h`)

**Stepper gauges**

```cpp
// Motor 1 – wind speed: 0–60 knots → 0 to 3/4 revolution
#define WIND_MIN_KT      0.0f
#define WIND_MAX_KT     60.0f
#define WIND_MAX_STEPS  (STEPS_PER_REV * 3 / 4)   // 3072 steps

// Motor 2 – pressure: 960–1040 hPa → 0 to 3/4 revolution
#define PRES_MIN_HPA   960.0f
#define PRES_MAX_HPA  1040.0f
#define PRES_MAX_STEPS (STEPS_PER_REV * 3 / 4)    // 3072 steps
```

**PWM wind speed meter (`SpeedMeter` class, GPIO22)**

The third instrument is an analog panel meter with a 0–300 mV (nominal) full-scale range, wired directly to GPIO22. The `SpeedMeter` class drives the pin with a 12-bit, 1 kHz LEDC PWM signal; the meter's own inertia filters the switching to a steady deflection.

| Parameter | Value |
|---|---|
| GPIO | 22 (`PWM_SPEED_PIN`) |
| LEDC resolution | 12-bit (4096 steps) |
| LEDC frequency | 1 kHz |
| Scale | 0–30 kn → 0–fullScaleMv |
| Default full-scale | 300 mV |
| Supply voltage used for duty calc | 3300 mV (3.3V rail) |

The full-scale voltage is user-adjustable via the `/calib` web page and persisted in NVS under the key `pwm_fs_mv`. Changing it does not require reflashing — save and the new calibration is applied immediately.

### 9.3 Main firmware features

**Boot sequence**

On every power-on both displays show the project name ("Teknosofen"), firmware
version (`FW_VERSION` in `config.h`), and the build date (stamped automatically
by the compiler via `__DATE__`). After 2 seconds the startup motor self-test runs
(see below), then another 2 seconds of splash before WiFi setup begins.
Bumping the version requires only changing `FW_VERSION`.

Once running, the two displays diverge: display 1 (clock face) shows the analogue
clock updated every second; display 2 (weather panel) shows the weather data layout
and refreshes after each 10-minute weather fetch.

**Startup self-test — motors and speed meter simultaneously**

Immediately after the splash screen appears, all three instruments perform a
coordinated self-test driven directly from the `setup()` loop:

- Both stepper needles sweep **+30°** (341 half-steps forward) then **−30°** back,
  returning to their exact pre-test positions. NVS-restored step counts remain valid.
- The PWM speed meter sweeps **150 → 200 → 100 → 150 mV** in sync with the motor
  movement (150→200 mV on the forward sweep, 200→100 mV on the backward sweep, then
  back to 150 mV at rest).

The interleaving is done step-by-step in `setup()` using `Instruments::stepBoth()`
and `SpeedMeter::setMillivolts()` in the same loop — no RTOS tasks are needed. Total
self-test duration is approximately 2 seconds (341 steps × 3 ms × 2 directions).

**Gauge position persistence (NVS)**

After every weather update and every calibration mode change, the current step
counts for both gauges are written to flash (ESP32 NVS, namespace `tjofia`,
keys `wind_steps` / `pres_steps`). On the next boot, these counts are read back
and passed to `Instruments::begin()`, which sets the internal position tracker
without moving the motors. The needles stay physically where they were when power
was cut; the firmware resumes with correct delta tracking from the first new
weather fetch.

**Wind speed tracking**

After each successful weather fetch `SpeedMeter::setKnots()` is called with the
current wind speed converted from m/s to knots. The duty cycle is computed as:

```
duty = (knots / 30.0) × (fullScaleMv / 3300.0) × 4095
```

The `/wx` status page shows the current output in mV, the equivalent knots, and the
calibrated full-scale value.

**Calibration mode (stepper gauges)**

Accessible from the `/wx` status page. When enabled, both stepper gauges move to
fixed reference positions (≈19.4 kn / 1000 hPa) and the speed meter is also driven
to the same reference wind speed. Step counts are persisted to NVS; the speed meter
position is not persisted (it is always derived from weather data or the reference
value on enable/disable). Live weather updates are suppressed while active.

**Speed meter calibration page (`/calib`)**

Accessible from the main config page. Allows setting the full-scale voltage (mV) —
the output that drives the meter to full-scale (30 kn) deflection. The value is
stored in NVS (`pwm_fs_mv`) and applied immediately without restart. More calibration
options will be added to this page in future firmware revisions.

### 9.4 Display rendering

**Erase-by-overwrite**

All moving elements (clock hands, temperature text) are erased by redrawing
them in the background colour (`COL_BG = 0x0000`) rather than by clearing
a rectangle with `fillRect()`. This avoids erasing neighbouring pixels and
eliminates the brief blank flash that `fillRect()` causes on the black display.

- **Clock hands** — each hand's previous pixel path is redrawn in `COL_BG`
  before the new position is drawn in its hand colour.
- **Second-hand tail** — the 20-pixel counter-balance stub is explicitly
  erased (it was previously leaked between frames because it lies outside the
  main-hand sweep).
- **Temperature text** — the previous formatted string is reprinted in `COL_BG`
  at its stored cursor position, then the new string is drawn in `COL_ACCENT`.
  If the formatted value is unchanged the function returns immediately with zero
  SPI traffic (~59 of every 60 ticks).

**Skipping unchanged hands**

The minute hand moves < 0.14 px/s at its 80-pixel length; the hour hand even
less. Both are compared pixel-endpoint before and after each tick — the
erase-and-redraw is skipped entirely when the integer endpoint has not moved.
This eliminates the sub-second hand flicker visible in earlier firmware.

**Bevelled bezel ring**

The clock face bezel is an 8-pixel wide ring drawn as 8 concentric circles
from `R+5` (almost black `0x0841`) to `R−2` (white `COL_FACE`), giving a
dark-to-bright gradient. The same `drawBezel()` helper is called by every
full-screen function (`showSplash`, `showStatus`, `showError`, `showAPMode`,
`drawFace`) so the look is consistent across all screens.

| Ring radius | RGB565 | Appearance |
|---|---|---|
| R+5 | `0x0841` | almost black (outermost) |
| R+4 | `0x1082` | very dark grey |
| R+3 | `0x2104` | dark grey |
| R+2 | `0x4208` | medium-dark grey |
| R+1 | `0x630C` | medium grey |
| R   | `0x8410` | medium-light grey |
| R−1 | `0xC618` | light grey |
| R−2 | `0xFFFF` | white (innermost) |

`R = 112`. `R+5 = 117` — safely within the GC9A01's 120 px circular clip.

**Tick marks and hand thicknesses**

| Element | Previous | Current |
|---|---|---|
| Minute tick | 1 px, grey (`0x7BEF`) | 2 px, white |
| Hour tick | 1 px, white | 3 px, white |
| Minute hand | 3 px wide | 5 px wide |
| Hour hand | 5 px wide | 7 px wide |
| Second hand | 1 px | 1 px (unchanged) |

Tick width is achieved by drawing an extra parallel line offset by one pixel
perpendicular to the radial direction. Hour ticks get an extra line on each
side (3 total); minute ticks get one additional line (2 total).

---

## 10. Practical Tips

- **Always call `off()`** when a motor is stopped. Coils stay energised otherwise,
  causing the motor to run warm and draw current needlessly.
- **Power motor coils from an external 5V supply.** The HUZZAH32 3.3V rail cannot
  supply the ~200 mA per coil the motors require.
- **Share GND** between the ESP32, both ULN2003A chips, the motors, and the 5V
  supply.
- **4096 half-steps = 360°** of the output shaft (internal 1:64 gear already included).
  2048 = 180°. 341 ≈ 30°. Full gauge scale (270°) = 3072 steps ≈ 15 s at 5 ms/step.
- **Reliable step delay is 3–5 ms.** Default in `Stepper28BYJ.h` is 5 ms. 3 ms works
  on confirmed hardware; do not go below 2 ms.
- **GPIO 16 and 17** (board labels RX/TX) are used for Motor 2. If you later need
  the hardware Serial1 port, reassign those motor pins to other free GPIOs.

---

## 11. Demo / Motor-Test Mode

The project includes a dedicated PlatformIO environment (`demo`) that exercises both
stepper motors through a three-phase test sequence without requiring WiFi, a working
display, or an OpenWeatherMap key. Use it to verify wiring and motor function before
final assembly.

### 11.1 PlatformIO Environment

The demo is compiled from `src/demo/main.cpp`. `build_src_filter` keeps the demo
and main firmware completely separate — they share only `config.h` and
`Stepper28BYJ.h`.

```ini
; platformio.ini (relevant excerpt)

[env:featheresp32]        ; main weather station firmware
extends = common
build_src_filter = +<*> -<demo/>
lib_deps = ...

[env:demo]                ; motor test — no WiFi or display needed
extends = common
build_src_filter = -<*> +<demo/>
```

To upload the demo from the command line:

```bash
pio run -e demo -t upload
```

Or select the **demo** environment in the PlatformIO IDE toolbar before clicking
Upload.

### 11.2 Pre-flight Requirement

> **Important:** Both motor needles must be at their physical minimum stop (zero
> position) before uploading the demo. The sequence tracks relative steps only and
> has no homing routine.

### 11.3 Test Sequence

The demo runs once in `setup()` then idles in `loop()`. Three phases:

| Phase | Description |
|---|---|
| **1 — Full-range sweep** | Each motor sweeps CW from zero to its maximum gauge position (3072 steps, ¾ rev) then returns CCW to zero. Confirms motor runs, wiring is correct, and full needle travel is unobstructed. |
| **2 — Quarter landmarks** | Both motors step to 25 %, 50 %, 75 %, and 100 % of full scale with a brief pause at each point, then return to zero. Useful for marking gauge face positions. |
| **3 — Fast oscillation ×5** | Both motors shuttle between zero and mid-scale at a 2 ms step delay (near-maximum speed). Stresses driver chips and coils; check for missed steps or overheating. |

### 11.4 Serial Output

Connect at **115200 baud**. Progress is printed for each phase:

```
=== Motor demo ===
Both needles should be at physical zero before flashing.

-- Phase 1: full range --
[wind] sweep CW  3072 steps
[wind] sweep CCW 3072 steps
[pres] sweep CW  3072 steps
[pres] sweep CCW 3072 steps

-- Phase 2: 25 / 50 / 75 / 100 % landmarks --
  25%
  50%
  75%
  100%

-- Phase 3: fast oscillation (5x) --

Demo complete. Needles should be at zero.
```

If a needle does not return to zero the motor likely missed steps — increase the
step delay (default 3 ms) or check for power supply sag on the 5V rail.

### 11.5 Switching Between Demo and Main Firmware

| Environment | CLI command | Purpose |
|---|---|---|
| `featheresp32` | `pio run -e featheresp32 -t upload` | Full weather station firmware |
| `demo` | `pio run -e demo -t upload` | Motor test only |

After testing, upload `featheresp32` to restore the full firmware. The two
environments are independent — neither overwrites the other's source files.
