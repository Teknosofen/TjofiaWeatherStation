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
| Steps / rev (half-step) | 4096 — output shaft, internal 1:64 gear included |
| Gear ratio | 1:64 internal (motor only — no external reduction) |
| Motor supply | 5V external — NOT the 3.3V board rail |

---

## 2. HUZZAH32 Pin Allocation

The HUZZAH32 uses an ESP32-WROOM32 module. Some board pins carry alternate labels
(A0–A5) that do not match the underlying GPIO number — the table below shows both.

| Board label | GPIO # | Assigned to | Notes |
|---|---|---|---|
| SCK | 5 | Display CLK | Board SPI SCK — hardware pin, fixed |
| MOSI | 18 | Display MOSI | Board SPI MOSI — hardware pin, fixed |
| D15 | 15 | Display CS | Chip select |
| D13 | 13 | Display DC | Data / command |
| A5 | 4 | Display RST | Reset |
| 14 | 14 | Motor 1 — IN1 | Wind gauge |
| 27 | 27 | Motor 1 — IN2 | Wind gauge |
| 32 | 32 | Motor 1 — IN3 | Wind gauge |
| 33 | 33 | Motor 1 — IN4 | Wind gauge |
| A1 | 25 | Motor 2 — IN1 | Pressure gauge — board label A1, actual GPIO 25 |
| A0 | 26 | Motor 2 — IN2 | Pressure gauge — board label A0, actual GPIO 26 |
| RX | 16 | Motor 2 — IN3 | Pressure gauge — safe if Serial1 unused |
| TX | 17 | Motor 2 — IN4 | Pressure gauge — safe if Serial1 unused |
| 34, 35, 36, 39 | — | Do not use for output | Input-only pins — no output driver |
| 6–11 | — | Do not use | Internal SPI flash — never expose |

> **Note:** The A0/A1 labels on the HUZZAH32 silk refer to GPIO 26 and 25
> respectively — not GPIO 0 and 1. Always use the GPIO number in code, not the
> A-prefix label.

> **Note:** RX (GPIO 16) and TX (GPIO 17) are usable as general outputs as long
> as you do not need the hardware Serial1 port.

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

**GC9A01 SPI Display**

| HUZZAH32 label | GPIO # | Connect to | Notes |
|---|---|---|---|
| SCK | 5 | Display CLK | Board SPI SCK |
| MOSI | 18 | Display MOSI | Board SPI MOSI |
| D15 | 15 | Display CS | |
| D13 | 13 | Display DC | |
| A5 | 4 | Display RST | |
| 3V | — | Display VCC | 3.3V |
| GND | — | Display GND | shared ground |

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
| Max SPI clock | 80 MHz (80 MHz used on ESP32 VSPI) |
| Driver IC | Sitronix GC9A01 |

### 6.1 SPI Signal Roles

| Pin | Direction | Function |
|---|---|---|
| CLK | ESP32 → display | SPI clock |
| MOSI | ESP32 → display | Serial data in (display has no MISO) |
| CS | ESP32 → display | Chip select — active LOW |
| DC | ESP32 → display | HIGH = data, LOW = command |
| RST | ESP32 → display | Hardware reset — active LOW pulse on startup |
| VCC | — | 3.3V supply |
| GND | — | Common ground |

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
| `/wx` | Status — live weather, time, location, gauge steps, calibration toggle |
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

The firmware determines latitude/longitude automatically via **ip-api.com**
(no key required). The result is cached in flash (NVS) so subsequent boots
are instant even if the service is temporarily unreachable.

### 7.4 NTP time sync

Local time is obtained from `pool.ntp.org` / `time.nist.gov` after WiFi
connects. The UTC offset is derived from the ip-api geolocation response,
so no manual timezone configuration is needed.

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
| `src/DisplayManager.h/cpp` | GC9A01 display rendering — DIYables_TFT_Round driver |
| `src/WeatherClient.h/cpp` | IP geolocation + OpenWeatherMap fetch |
| `src/main.cpp` | State-machine entry point |
| `src/demo/main.cpp` | Motor test sketch (see §9) |

**Stepper28BYJ** is self-contained — no external stepper library needed.
**StepperGauge** wraps it with a value-to-steps mapping and tracks the current
needle position so only the delta is driven on each update.

### 9.1 Display driver (`platformio.ini`)

```ini
lib_deps =
    https://github.com/DIYables/DIYables_TFT_Round.git
```

The library extends **Adafruit GFX**, so all standard Adafruit GFX drawing and font
functions are available. The constructor takes `(RST, DC, CS)`; the SPI clock and
data lines are fixed by the board variant.

```cpp
// DisplayManager.cpp — initialisation
// CLK = GPIO5 (board "SCK"), MOSI = GPIO18 (board "MOSI") — fixed by board definition
DIYables_TFT_GC9A01_Round _tft(TFT_RST, TFT_DC, TFT_CS);  // RST=4, DC=13, CS=15
_tft.begin();
```

> **Note:** The HUZZAH32 routes its SPI bus to SCK=GPIO5 and MOSI=GPIO18 — these
> differ from the raw ESP32 VSPI defaults (CLK=18, MOSI=23). The library picks up
> the correct pins automatically via the board's `SPI.begin()`. Only RST, DC, and
> CS are free to assign.

### 9.2 Gauge physical limits (`config.h`)

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
