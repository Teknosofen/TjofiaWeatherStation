# 28BYJ-48 Dual Stepper Motor Control
**ULN2003A Driver · Adafruit HUZZAH32 (ESP32-WROOM) · GC9A01 SPI Display**

---

## Quick Start

This section gets a freshly flashed TjofiaWX station running in about five minutes.
The detailed reference sections that follow cover wiring, pin assignments, and
firmware internals.

### What you need

| Item | Notes |
|---|---|
| Assembled TjofiaWX hardware | HUZZAH32 + both GC9A01 displays + stepper gauges + PWM meter wired per §2–§5 |
| USB cable or 5V supply | HUZZAH32 USB for power; stepper motors need a separate 5V rail |
| Home WiFi network | 2.4 GHz band; WPA/WPA2 personal |
| OpenWeatherMap API key | Free account at [openweathermap.org](https://openweathermap.org) → My Profile → API keys |
| Phone, tablet, or laptop | Any device with a browser; must support WiFi |

---

### Step 1 — Power on

Apply power. Both circular displays show the **Teknosofen splash** (project name,
firmware version, build date). After ≈ 2 seconds the startup self-test moves all
instrument needles briefly and the clock display transitions to the WiFi setup screen.

**If WiFi credentials are already saved** the device connects directly and you
can skip to Step 5.

---

### Step 2 — Connect to the setup hotspot

The clock display shows four lines:

```
WiFi Setup
Connect to:
TjofiaWX-Setup
192.168.4.1
```

On your phone or laptop, open the WiFi settings list and connect to
**TjofiaWX-Setup** (open network — no password required). Then open a browser
and go to **http://192.168.4.1** — a setup page loads.

---

### Step 3 — Enter WiFi credentials and OWM key

The setup page lists nearby WiFi networks. Select your home network, enter its
password, and paste your **OpenWeatherMap API key** into the field at the bottom.
Tap **Save**. The device connects to your home network; the setup hotspot closes.

> **Can't see a network?** Tap *Scan* or scroll down — the list auto-refreshes.
> The HUZZAH32 only supports 2.4 GHz; 5 GHz networks will not appear.

---

### Step 4 — Read the IP address from the clock display

Once connected to your home network the clock display shows two lines:

```
YourNetworkName
192.168.x.x
```

**Line 1** is the name (SSID) of your home WiFi.
**Line 2** is the IP address the router assigned to TjofiaWX.

Make a note of the IP address — it lets you reach the web portal from any browser
on the same network even if mDNS is not available.

The display then steps through *NTP syncing…*, *Locating…*, and *Fetching weather…*
before the analogue clock face appears and the weather panel shows live data.

---

### Step 5 — Open the web portal

From any browser on your home network, open one of:

| Address | When to use |
|---|---|
| `http://TjofiaWX.local` | Works on macOS, iOS, Windows 10 1903+; may not work on Android |
| `http://192.168.x.x` | Always works — use the IP shown on the clock display in Step 4 |

The portal home page shows four large buttons:

| Button | Page | What you do there |
|---|---|---|
| ☁ Weather | `/wx` | View live data; update or change the OWM API key |
| 📍 Set Location | `/location` | Pin exact lat/lon on a map for precise local weather |
| ⚙ Calibration | `/calib` | Calibrate the stepper gauges and speed meter |
| 🖼 Images | `/images` | Upload photos that cycle on the weather display |

---

### Step 6 — (Optional) Pin your location

IP geolocation resolves to the ISP's exchange, which can be tens of kilometres
from the device. For accurate local weather data, open the portal and tap
**📍 Set Location**:

1. The map opens centred on the auto-detected position.
2. Click the map or drag the pin to your exact location.
3. Tap **Use my GPS** on a phone to jump the map to your GPS position.
4. Tap **Save & pin** — a new weather fetch starts within 30 seconds.

---

### Step 7 — Done

| Display | What you see | Update rate |
|---|---|---|
| Clock (left) | Analogue clock face + today's date below centre | Every second |
| Weather (right) | Temperature, feels-like, wind direction & speed, pressure, humidity | Every 10 minutes |

The weather display also cycles through any uploaded photos: weather panel →
photo 1 → weather panel → photo 2 → … (default interval 10 s, adjustable on
the Images page).

If the clock display ever shows **No weather data / Check setup**, open
`http://TjofiaWX.local/wx`, verify the OWM key, and check that the device has
internet access.

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
| Gear ratio | 1:64 internal per motor; motor 2 (pressure) adds an external reduction stage |
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
| 23 | 23 | Display 2 CS | Weather display — chip select (`TFT2_CS`) |
| 22 | 22 | PWM speed out | Electrical speed indicator output (`PWM_SPEED_PIN`) |
| 14 | 14 | Motor 1 — IN1 | Wind direction gauge |
| 27 | 27 | Motor 1 — IN2 | Wind direction gauge |
| 32 | 32 | Motor 1 — IN3 | Wind direction gauge |
| 33 | 33 | Motor 1 — IN4 | Wind direction gauge |
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
| CS | — | GPIO 15 | GPIO 23 |

Wire the second display's CLK, MOSI, DC, and RST to the **same** board pins as
the first display. Connect only CS to GPIO 23.

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

**Motor 1 (Wind direction gauge)**

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
| 23 | 23 | CS | Display 2 — weather panel |

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
| `/images` | Image gallery — upload photos for the weather display boot screen; browser converts any format to RGB565 automatically |
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

### 7.5 Boot image (weather display)

The `/images` page lets you upload a photo that appears on the weather display at
startup instead of the Teknosofen splash. The conversion from any photo format to
240×240 RGB565 happens entirely in the browser — no Python, no PC tool, no
pre-processing step needed.

**Uploading a photo:**

| Step | Action |
|---|---|
| 1 | Open `http://TjofiaWX.local/images` |
| 2 | Tap **Choose photo** and pick any JPEG, PNG, HEIC, BMP, etc. |
| 3 | A circular 240×240 preview appears instantly (centre-crop + resize done in the browser) |
| 4 | Tap **Convert & Upload** — the browser encodes RGB565 big-endian and POSTs 115,200 bytes directly to the device |
| 5 | The gallery updates; tap **Set as boot** to use the image at next power-on |

**Gallery management:**

- Each stored image is shown as a circular 80×80 thumbnail (decoded from raw RGB565 by the same browser canvas)
- **Set as boot** — marks the image as the startup image; path stored in NVS (`boot_img` key)
- **Delete** — removes the file from LittleFS; clears boot-image NVS key if it pointed to that file
- **Clear (use splash)** — reverts to the Teknosofen splash without deleting any image

**Auto-selection:**

If no boot image is pinned in NVS, the firmware scans LittleFS on every boot and
uses the first `.raw` file it finds. Pin an explicit boot image with **Set as boot**
to make the choice deterministic when multiple images are stored.

**Storage limits:**

Images are stored as raw 240×240 RGB565 big-endian files (115,200 bytes each). The
LittleFS partition is 1 MB — this allows up to ~8 images with headroom for the
filesystem metadata. Any upload whose final size is not exactly 115,200 bytes is
deleted again and reported as a failure, so a truncated transfer cannot leave a
corrupt file in the gallery.

### 7.6 Slideshow (weather display)

With at least one stored image, the weather display alternates between live
weather and the gallery:

```
weather → photo 1 → weather → photo 2 → … → weather → photo N → weather → photo 1 → …
```

Each slide stays up for the interval set at the bottom of `/images` (1–300 s,
default 10 s, stored in NVS under `slide_sec`).

Timing is owned by the `Slideshow` class:

- It does nothing until the **first successful weather fetch** calls `start()`, so
  the boot image stays on screen through WiFi setup, geolocation and NTP sync.
- Every fresh weather fetch forces the weather slide back on screen and restarts
  the dwell timer, so newly fetched data is always visible for a full interval.
- Saving a new interval restarts the timer immediately rather than at the end of
  the current slide.
- It is skipped entirely while calibration mode or the gauge wizard is active,
  and resumes when either hands control back.

**Alternative (PC-based conversion):**

`tools/convert_image.py` (Pillow required) still works for cases where the browser
cannot decode a particular format (some HEIC files on Android). It produces an
identical `.raw` file that can be uploaded via the same `/images` page:

```bash
python tools/convert_image.py photo.jpg          # saves photo.raw
python tools/convert_image.py photo.jpg --preview  # preview before saving
```

Run without arguments on Windows/macOS to open a file picker dialog.

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

**Hardware drivers**

| File | Purpose |
|---|---|
| `src/Stepper28BYJ.h` | Low-level half-step driver — one instance per motor |
| `src/Instruments.h/cpp` | `StepperGauge` and `Instruments` — position-aware gauge abstraction |
| `src/SpeedMeter.h/cpp` | 12-bit LEDC PWM driver for the analog wind-speed meter panel |
| `src/BaseDisplay.h/cpp` | Abstract base — owns `_tft`, bezel, boot screens (splash/status/error/AP) |
| `src/ClockDisplay.h/cpp` | Extends `BaseDisplay` — analogue clock face, hands, centre temperature |
| `src/WeatherDisplay.h/cpp` | Extends `BaseDisplay` — weather data panel (temp, wind, pressure, humidity) |
| `src/DisplayManager.h` | Thin alias: `typedef ClockDisplay DisplayManager` (kept for compatibility) |

**Services**

| File | Purpose |
|---|---|
| `src/Settings.h/cpp` | The only owner of NVS — one named accessor per persisted value |
| `src/ImageStore.h/cpp` | LittleFS `.raw` gallery: enumerate, delete, stream, upload sink |
| `src/TimeService.h/cpp` | NTP sync (POSIX tz string construction) and local-clock readout |
| `src/Slideshow.h/cpp` | Weather ⇄ photo rotation on the weather display |
| `src/GaugeCalWizard.h/cpp` | Two-point stepper calibration state machine and linear fit |
| `src/WeatherClient.h/cpp` | IP geolocation + OpenWeatherMap fetch + Nominatim reverse geocoding |
| `src/NetPortal.h/cpp` | WiFiManager credential portal, persistent soft-AP, captive DNS, mDNS |

**Web interface**

| File | Purpose |
|---|---|
| `src/WebUI.h/cpp` | Owns the `WebServer`, shared page chrome (`pageHead`, `redirect`), route wiring |
| `src/WebPages.h` | One `register*()` declaration per page group |
| `src/WebPageHome.cpp` | `/` · `/save` · `/cal` · `/reset` |
| `src/WebPageStatus.cpp` | `/wx` status table |
| `src/WebPageLocation.cpp` | `/location*` Leaflet map pin |
| `src/WebPageCalib.cpp` | `/calib*` reference position, gauge wizard, speed-meter full scale |
| `src/WebPageImages.cpp` | `/images*` gallery, browser-side converter/upload, boot image, slideshow |

**Top level**

| File | Purpose |
|---|---|
| `src/config.h` | All pin assignments, gauge limits, NVS keys, timing constants |
| `src/AppContext.h/cpp` | The single `app` object: module instances, live state, shared actions |
| `src/main.cpp` | Boot sequence and top-level state machine — nothing else |

**Stepper28BYJ** is self-contained — no external stepper library needed.
**StepperGauge** wraps it with a value-to-steps mapping and tracks the current
needle position so only the delta is driven on each update.

### 9.0 Module layering

`main.cpp` holds only `setup()`, `loop()` and the boot state machine. Everything
else is a module with one responsibility, wired together through a single shared
object.

```
main.cpp          setup() + loop() + State{BOOT…RUNNING}
   │
   ├── AppContext  ── the one `app` global: owns every module instance,
   │                  the live GeoInfo/WeatherData, and the actions the
   │                  web layer triggers (applyCalMode, refreshAfterCalib,
   │                  saveGaugePositions, scheduleWeatherRefresh…)
   │
   ├── NetPortal   ── WiFi → soft-AP → DNS → mDNS → WebUI::begin()
   │      └── WebUI ── WebServer + page chrome
   │             └── WebPage*.cpp  (each registers its own routes)
   │
   └── services: Settings · ImageStore · TimeService · Slideshow ·
                 GaugeCalWizard · WeatherClient
       drivers:  Instruments · SpeedMeter · Clock/WeatherDisplay
```

Two rules keep the layering honest and are worth preserving:

- **`Settings` is the only file that touches `Preferences`/NVS**, and
  **`ImageStore` is the only file that enumerates LittleFS.** (`BaseDisplay`
  still opens an image file directly to render it — that is rendering, not
  storage management.) Every raw `NVS_*` key from `config.h` is referenced
  from `Settings.cpp` alone.
- **Web handlers never block on hardware they can defer.** Toggling the
  reference position posts to `/cal`, which only records the request via
  `AppContext::requestCalMode()`; `loop()` picks it up and drives the motors
  after the HTTP response has gone out. Motor moves take seconds — doing them
  inside a handler would stall the web server.

Long-running gauge ownership is arbitrated through two flags: `app.calMode`
(needles parked at reference values) and `app.gaugeCal.active()` (the wizard
holds the needles). While either is set, weather fetches still run and are
logged, but the gauges and the slideshow are left untouched.

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
static WeatherDisplay weatherDisp(TFT2_CS);   // CS=23, rst=-1 (no RST toggle)
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
// Motor 1 – wind direction: circular dial, 0–360° = one full output-shaft turn
#define WDIR_MIN_DEG     0.0f
#define WDIR_MAX_DEG   360.0f
#define WDIR_MAX_STEPS  STEPS_PER_REV             // 4096 steps = 360° of compass

// Motor 2 – pressure: 960–1040 hPa → 0 to 3/4 revolution
#define PRES_MIN_HPA   960.0f
#define PRES_MAX_HPA  1040.0f
#define PRES_MAX_STEPS (STEPS_PER_REV * 3 / 4)    // 3072 steps
```

**These constants are factory defaults, not limits.** `_maxSteps` seeds the
default gain (steps per unit) and nothing else — the step position is never
clamped to it:

- `StepperGauge::valueToSteps()` returns the raw calibrated target. It used to
  cap the result at `_maxSteps`, which froze a needle at 3072 steps as soon as
  a calibrated gain asked for more.
- `StepperGauge::nudge()` has no end stop, so the calibration wizard can drive a
  needle anywhere on the dial, past full scale and across several revolutions.
- `_pos` free-runs and is persisted as-is. It is never wrapped or clamped.

**Gearbox backlash (both motors)**

Every 28BYJ-48 has an internal 1:64 reduction gearbox, so **both** needles are
driven through a gear train with lost motion. Where a needle physically sits
therefore depends on which direction it was last driven — the same commanded
step count puts it in two different places depending on history.

| Gauge | Gear train | Lost motion | Compensation |
|---|---|---|---|
| Motor 2 — pressure | internal 1:64 **plus an external reduction stage** | 3–4 mBar = 115–154 steps at 38.4 steps/hPa | `PRES_BACKLASH_STEPS` = 250 (≈ 6.5 hPa) |
| Motor 1 — wind direction | internal 1:64 only | subset of motor 2's, so fewer steps — **not yet measured** | `WDIR_BACKLASH_STEPS` = 120 (≈ 10.5°) |

The pressure gauge is the worse case: a 1 hPa weather change is only 38 steps,
well inside its dead zone, so without compensation small pressure movements
would not move the needle at all and larger ones would arrive late.

`StepperGauge` handles this by **always approaching the target from below**.
`setValue()` drives to `target − backlashSteps`, then comes back up onto the
target, so the final leg is always increasing and the slack is always taken up
the same way:

```cpp
void StepperGauge::approachFromBelow(int target, int stepDelay) {
    moveTo(target - _backlashSteps, stepDelay);
    moveTo(target, stepDelay);              // final leg always increasing
    motor.off();
}
```

Three properties of this scheme are worth understanding:

- **The constant only has to exceed the real backlash.** It is not a precision
  figure — 250 steps against motor 2's measured 115–154 leaves ample margin.
  This is why undershoot is preferred over a model-based directional offset,
  which would need an accurate backlash figure that in practice varies with
  angle, load, temperature and wear.
- **It runs unconditionally** — on upward moves and even when the needle is
  already on target. That means nothing outside `setValue()` can leave the gear
  train in the wrong state (the startup self-test and the wizard's `nudge()`
  both finish wherever they happen to finish, and the slack state after a power
  cycle is unknowable). It also gives the barometer a visible dip-and-return on
  every fetch, which is welcome on a gauge whose reading can sit unchanged for
  hours.
- **The calibration must be taken the same way.** The stored coefficients
  describe the needle position with the slack taken up in the increasing
  direction, so each wizard mark must be approached with the **+** buttons. The
  `/calib` page says so in both wizard states. `nudge()` is deliberately
  uncompensated — the operator has to see raw needle motion.

Cost is ~500 extra steps (≈ 2.5 s at 5 ms/step) per fetch, once every 10 minutes.

Passing `backlashSteps = 0` disables all of the above and restores a straight
move to target — correct only for a genuinely direct-drive needle, which neither
of these is.

`WDIR_BACKLASH_STEPS` is an **unverified upper-bound estimate**. To measure the
real figure: open `/calib`, start the wind-direction wizard, drive the needle up
to a dial mark with the **+** buttons, then nudge **−10** repeatedly and count
the steps until the needle first visibly moves. Trim the constant to just above
that. Too large only costs travel time and a more visible dip, so erring high is
safe.

The compensation composes with circular shortest-arc movement at no extra cost:
`setValue()` resolves the shortest arc into an absolute target *first*, then
undershoots that. A compass needle never takes the long way round to get its
approach direction right — it dips past the target and comes back, the same as
the barometer.

Motor 1 is constructed with `circular = true`. A circular gauge takes the
**shortest arc** to its target: current and target positions are reduced modulo
one full scale, and the difference is wrapped to ±½ scale, so the needle never
unwinds the long way round from 350° to 10°. The modulus is derived from the
*calibrated* gain (`stepsPerUnit × (max − min)`), not from `_maxSteps`, so a
re-calibrated dial wraps at its true revolution.

> Because motor 1 is a full-circle compass, `WDIR_MAX_STEPS` is a whole
> revolution (4096). It was `STEPS_PER_REV * 3 / 4` while motor 1 still drove a
> 270° wind-*speed* arc; leaving it at 3072 made an uncalibrated compass wrap
> after ¾ of a turn. A stored calibration in NVS overrides the default either way.

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

**Persisted values (NVS namespace `tjofia`)**

Every one of these is read and written exclusively through `Settings`; the key
names themselves live in `config.h` and appear nowhere else but `Settings.cpp`.

| Key | Type | Written by | Meaning |
|---|---|---|---|
| `wind_steps` / `pres_steps` | int | weather update, cal-mode change, wizard exit | last known needle positions |
| `wdir_zero` / `wdir_gain` | float | gauge wizard | wind-direction calibration (steps at 0°, steps per °) |
| `pres_zero` / `pres_gain` | float | gauge wizard | pressure calibration (steps at 960 hPa, steps per hPa) |
| `pwm_fs_mv` | float | `/calib` | speed-meter full-scale voltage |
| `owm_key` | string | `/save`, WiFiManager portal | OpenWeatherMap API key |
| `lat` / `lon` | float | geolocation, `/location/save` | station coordinates |
| `loc_pinned` | bool | `/location/save`, `/location/clear` | ignore IP geolocation for coordinates |
| `timezone` / `utc_off` | string / int | geolocation | tz name and UTC offset in seconds |
| `boot_img` | string | `/images/setboot` | pinned boot image path |
| `slide_sec` | int | `/images/slide-save` | slideshow interval in seconds |

**Wind speed tracking**

After each successful weather fetch `SpeedMeter::setKnots()` is called with the
current wind speed converted from m/s to knots. The duty cycle is computed as:

```
duty = (knots / 30.0) × (fullScaleMv / 3300.0) × 4095
```

The `/wx` status page shows the current output in mV, the equivalent knots, and the
calibrated full-scale value.

**Calibration mode (reference position)**

The checkbox at the top of `/calib`. When enabled, both stepper gauges move to
fixed reference positions (North / 1000 hPa, from `CAL_WDIR_DEG` and
`CAL_PRES_HPA`) and the speed meter is driven to `CAL_WIND_MS`. Step counts are
persisted to NVS; the speed meter position is not persisted (it is always derived
from weather data or the reference value on enable/disable). Live gauge updates
are suppressed while active.

The POST handler does **not** move the motors — it calls
`AppContext::requestCalMode()` and returns immediately, and `loop()` performs the
move on the next pass. Otherwise the browser would wait out a multi-second
stepper run.

**Gauge calibration wizard (`/calib`)**

Two-point linear calibration, one gauge at a time, implemented by
`GaugeCalWizard`. The operator nudges the needle to a printed mark (±1 / ±10 /
±100 / ±1000 steps, over AJAX so the page does not reload), types the value that
mark represents, and repeats for a second mark. From the two (steps, value) pairs
the wizard derives:

```
gain = (steps₂ − steps₁) / (value₂ − value₁)          // steps per unit
zero = steps₁ − gain × (value₁ − scaleMinimum)        // steps at scale minimum
```

> **Approach every mark from below.** Finish each adjustment with the **+**
> buttons; if you overshoot, back off well past the mark and come up again. The
> pressure gauge has 3–4 mBar of gear backlash and the firmware always drives it
> upwards onto its target, so a calibration taken on a descending approach
> describes a needle position the firmware never reproduces. The `/calib` page
> repeats this warning in both wizard states.

Pairs that would give a zero or negative gain are rejected and logged. Accepted
coefficients go to NVS (`wdir_zero`/`wdir_gain`, `pres_zero`/`pres_gain`) and are
reloaded on boot; **Reset** discards them and restores the factory gain derived
from `*_MAX_STEPS`. On boot a stored gain more than 2.5× or less than 0.4× the
factory value is flagged on the serial console — it usually means a mis-clicked
wizard rather than an unusual dial.

While the wizard is active the reference-position checkbox is disabled and the
weather fetch leaves the gauges alone. The **Back** button posts to
`/calib/gc-back`, which cancels the wizard before navigating home — otherwise the
gauges would stay frozen.

**Speed meter calibration (`/calib`)**

Sets the full-scale voltage (mV) — the output that drives the meter to full-scale
(30 kn) deflection. Accepted range 50–3300 mV. The value is stored in NVS
(`pwm_fs_mv`) and applied immediately without restart; the needle re-deflects to
the current wind speed as soon as it is saved.

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
  2048 = 180°. 1024 = 90°. 341 ≈ 30°. A full revolution takes ≈ 20 s at 5 ms/step,
  a 270° arc (3072 steps) ≈ 15 s.
- **Reliable step delay is 3–5 ms.** Default in `Stepper28BYJ.h` is 5 ms. 3 ms works
  on confirmed hardware; do not go below 2 ms.
- **GPIO 16 and 17** (board labels RX/TX) are used for Motor 2. If you later need
  the hardware Serial1 port, reassign those motor pins to other free GPIOs.
