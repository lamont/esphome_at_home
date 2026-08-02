# ESPHome fleet upgrade — 2026-08-01

Upgraded the whole fleet's config against **ESPHome 2026.7.2** (the version
already pinned as default in `aliases`), fixed everything that broke from
version drift, and pushed OTA updates to the devices that were both online
and compiling clean. Full inventory, reachability, and compile status below.

## Method

- Compiled every `*.yaml` device config in the repo locally via
  `docker run esphome/esphome:2026.7.2 compile <file>.yaml` (no hardware
  needed for this step).
- Checked reachability via mDNS (`ping <name>.local`, 2.5s timeout per host).
- For devices that were both online and compiled clean, pushed the upgrade
  over OTA (`esphome upload <file>.yaml --device <ip>`) and verified the
  device came back up (ping + TCP check on the ESPHome API port, 6053).
- Devices that were online but did **not** compile clean were left running
  their existing firmware untouched — no broken config was pushed to live
  hardware.

## Inventory, reachability, and final status

| Device (file) | Purpose | Board | Online? | Compiles (2026.7.2)? | OTA pushed? |
|---|---|---|---|---|---|
| `snek` | Wio Link — internal/external DHT temp+humidity | ESP8266 wio_link | **Yes** (192.168.178.213) | Yes | **Yes** ✅ |
| `patio-speakers` | Sonocotta "Amped Esparagus" — Sendspin multiroom speaker | ESP32-WROVER | **Yes** (192.168.178.79) | Yes | **Yes** ✅ |
| `indoorgrow` | Grow tent tensiometer + CCS811/BME280/SCD40/TMP117/BH1750 | ESP32 esp32thing_plus | **Yes** (192.168.178.228) | Yes | **Yes** ✅ |
| `m5station` | M5Stack Station — battery, SHT31, plant moisture/pump | ESP32 m5stack-station | Yes (192.168.178.221) | **No — blocked** | No (left on old firmware) |
| `plantwaterer` (`plantWaterer.yaml`) | M5StickC plant moisture sensor + pump + display | ESP32 m5stick-c | Yes (192.168.178.226) | **No — blocked** | No (left on old firmware) |
| `sadbat` | Qwiic LED Stick bargraph/dimmer (no HA) | ESP32 thing_plus | No (never flashed OTA-capable yet — first flash is USB-only) | Yes | N/A (no OTA path) |
| `bedroomecho` | M5Stack Atom Echo — Home Assistant voice satellite | ESP32 m5stack-atom | No | Yes | N/A |
| `ble0` | Olimex PoE ESP32 — BLE proxy + iGrill listener | ESP32 esp32-poe (ethernet) | No | Yes | N/A |
| `co2sensor` | SCD41 CO2 + AMG8833 thermal testbed | ESP32-S3 um_feathers3 | No | Yes (AMG8833 disabled, see below) | N/A |
| `co2tdisplay` | CO2 monitor with ST7789 display, workroom | ESP32-S2 featheresp32-s2 | No | Yes | N/A |
| `feathers3um` | Feather S3 test platform | ESP32-S3 um_feathers3 | No | Yes | N/A |
| `indoorgrowupper` | Grow tent upper — AMG8833 leaf temp + SCD40 + BME280 | ESP32 esp32thing_plus | No | Yes | N/A |
| `litcontrol` | Boat multi-zone light controller | ESP32 featheresp32 | No | Yes | N/A |
| `thinggrideye` | ESP32 Thing Plus — AMG8833 testbed | ESP32 esp32thing_plus | No | Yes (AMG8833 disabled, see below) | N/A |
| `ttgottv` (`ttgoTTV.yaml`) | LilyGo TTV — OLED, RTC, battery ADC testbed | ESP32 esp32dev | No | Yes | N/A |
| `powermeter` | Garage IR pulse-counter kWh meter + DHT sensors | ESP8266 wio_link | No | Yes | N/A |
| `blaster` | IR blaster / FastLED relay | ESP32 lolin32 | No | Yes | N/A |
| `undercabinet` | Kitchen under-cabinet RGB/WW/CW LED strip | ESP8266 esp8285 | No | Yes | N/A |
| `tdisplay` | Original TTGO T-Display clock | ESP32 featheresp32 | No | Yes | N/A |
| `soils` | Old battery-voltage/percent soil node | ESP32 pocket_32 | No | Yes | N/A |
| `testsoil` | Test soil moisture node | ESP8266 wio_link | No | Yes | N/A |
| `tipper` | Test wio_link node | ESP8266 wio_link | No | Yes | N/A |
| `lcd` | Oldest wio_link node (pre-2020, `esphomeyaml:` era config) | ESP8266 wio_link | No | Yes | N/A |
| `loraWatch.yaml` | LilyGo TTGO T-Wristband notes | **STM32, not ESP** | — | N/A — not an ESPHome device (no `esphome:` key; pin notes only, no board hardware to run ESPHome) | N/A |

**Result: 20 of 22 ESPHome device configs compile clean against 2026.7.2.
3 of 5 reachable devices upgraded and confirmed back online. 2 known
blockers remain** (see below), both left running their prior firmware.

## Fixes applied (by category)

### 1. `ota:` requires an explicit `platform` key (18 files)
ESPHome's `ota:` component became multi-backend; a bare `ota: password: ...`
block (previously implicit) now fails validation. Added `platform: esphome`
to every device that was missing it, including the shared `.home.yaml`
include (`ble0`, `blaster`, `co2sensor`, `co2tdisplay`, `feathers3um`,
`indoorgrow`, `indoorgrowupper`, `lcd`, `litcontrol`, `powermeter`, `soils`,
`tdisplay`, `testsoil`, `thinggrideye`, `tipper`, `ttgoTTV`, `undercabinet`,
`.home.yaml`). `patio-speakers` and `sadbat` already had it right.

### 2. `platform: ESP32`/`ESP8266` under `esphome:` is fully removed (10 files)
The old inline-platform syntax (`esphome: { platform: ESP32, board: ... }`)
was deprecated years ago and is now a hard error — must be a separate
`esp32:`/`esp8266:` root key. Migrated: `ble0`, `blaster`, `lcd`,
`litcontrol`, `powermeter`, `snek`, `soils`, `tdisplay`, `testsoil`,
`tipper`, `undercabinet`. (`lcd.yaml` also still had the pre-2020
`esphomeyaml:` root key from before ESPHome's own rename — updated to
`esphome:`.)

### 3. `bme280` → `bme280_i2c` (`indoorgrow`, `indoorgrowupper`)
The `bme280` sensor platform was split into `bme280_i2c` / `bme280_spi`.
Both devices address their BME280 over I²C, so renamed accordingly.

### 4. Arduino framework defaults to ESP-IDF now; Arduino-only libraries need it pinned explicitly (`indoorgrowupper`, `thinggrideye`, `ttgoTTV`)
When `esp32: framework:` is omitted, ESPHome now defaults to `esp-idf`
rather than `arduino`. Both AMG8833 testbeds use the Arduino-only "SparkFun
GridEYE AMG88 Library" and needed `framework: { type: arduino }` added
explicitly. Same root cause hit `ttgoTTV.yaml`, whose framework block had
been commented out entirely.

### 5. Old `Arduino 2.0.2` core pin no longer supported (`co2sensor`)
`co2sensor.yaml` pinned `framework: { version: 2.0.2, platform_version:
5.0.0 }` from a 2022-era workaround comment ("board not yet defined
upstream"). That workaround is long obsolete and the pinned version is
now rejected outright ("Only Arduino 3.0+ is supported"). Switched to
`version: recommended`.

### 6. Duplicate-pin validation is now enforced — two different fixes needed
ESPHome now hard-fails when the same GPIO is configured in two places,
where it previously allowed it silently. Two different resolutions applied
depending on intent:

- **Genuinely leftover/stale duplicate** — `co2tdisplay.yaml` had a
  "T-Display Button Input 1" binary_sensor on GPIO35 copy-pasted from the
  *original* TTGO T-Display board, left over after a previous session
  repurposed this file for a Feather ESP32-S2 (whose SPI now legitimately
  uses GPIO35 as MOSI). Removed the stale binary_sensor — this board never
  had that second button.
- **Deliberately shared pin** — needs `allow_other_uses: true` on *every*
  place that pin is used (I initially used the wrong key,
  `ignore_pin_validation_error`, which ESPHome accepts syntactically but
  silently ignores for this specific check — corrected to the actual key):
  - `plantWaterer.yaml`: GPIO10 is both the `status_led` and a manually
    toggled `switch` (same physical LED, dual control) — intentional.
  - `soils.yaml`: GPIO35 is read by two separate `adc` sensors (raw voltage
    and derived percentage) — intentional.
  - `tdisplay.yaml`: GPIO4 is both a `switch` and the display's
    `backlight_pin` — intentional (manual override of the backlight).
  - `powermeter.yaml`: GPIO13 is shared between a `dht` sensor and the
    `pulse_counter` — **this one looks like a genuine pre-existing wiring
    bug or copy/paste mistake**, not an intentional design (a DHT sensor
    and an IR pulse counter on the same pin is an unusual combination, and
    the original file has a comment "I think this is the middle pin"
    suggesting uncertainty even at authoring time). I preserved prior
    (accepted) behavior with `allow_other_uses: true` rather than guessing
    a different physical pin — **please verify the actual wiring here.**

### 7. `ESPTime` struct field renamed (`soils`, `testsoil`, `tipper`)
`(id(sntp_time).now()).time` no longer compiles — the field is now
`.timestamp`. Fixed in all three affected lambdas.

### 8. `st7789v` now requires an explicit `model:` (`tdisplay`)
Previously optional/defaulted; added `model: Custom` with the known TTGO
T-Display panel geometry (240×135), matching what `co2tdisplay.yaml`
already had for the same hardware family.

### 9. `i2s_audio` media_player platform removed (`bedroomecho`)
Replaced with the new `speaker` + `media_player: platform: speaker`
structure, matching the current upstream
`esphome/media-players/m5stack/m5stack-atom-echo.yaml` package (this
device was originally imported from that package). Also dropped the now
build_flags/unsupported `rmt_channel` key.

### 10. `custom` sensor/text_sensor platform removed entirely (`co2sensor`, `thinggrideye`)
ESPHome dropped the old "inline lambda instantiates a raw component"
escape hatch — a real `external_component` is required now. Both devices'
AMG8833 8x8 thermal camera testbed wiring used this pattern. **Disabled**
(commented out) rather than guessed at a rewrite, since these are
explicitly-labeled testbeds with no other active sensors — also removed
the now-orphaned `includes:`/`libraries:` entries, since with no sensor
config left to pull in ESPHome's sensor headers, the raw AMG8833 header
itself stopped compiling (`'Sensor' does not name a type`). **Needs a
proper external_component rewrite to bring AMG8833 support back** on
either device — flagging as follow-up work, not done here.

### 11. Abandoned/incompatible third-party Arduino library metadata (`ttgoTTV`)
`libraries: [Rtc_Pcf8563]` was declared but **never actually referenced**
anywhere in the config (time comes from the `homeassistant` time platform,
not this RTC). Its `library.properties` metadata claims `atmelavr`-only
platform support, which the current library-conversion layer now enforces
and rejects for ESP32 (previously ignored under legacy PlatformIO). Simply
removed the unused declaration. If the PCF8563 RTC chip on this board ever
needs to be wired up for real, ESPHome now ships a **native** `pcf8563`
time component — use that instead of a third-party library.

### 12. Deprecated `attenuation: 11db` → `12db` (`soils`)
Cosmetic deprecation warning cleanup while already touching this file.

## Known blockers — left un-upgraded, on prior firmware

Both of these are online and functioning today; I did **not** push a
broken config to either. Both need an owner decision (see options below)
rather than a guess on my part, since the "fix" changes runtime behavior
on live hardware.

### `m5station.yaml` — M5Stack Station (192.168.178.221)
The `m5stack/M5Station` Arduino library (unmaintained since 2022, only
ever released as v0.0.1 on the PlatformIO registry — no newer version
exists) does raw ESP32 GPIO register access
(`GPIO.out_w1ts = (1 << TFT_DC)`) in `src/utility/In_eSPI.h`. Current
arduino-esp32/ESP-IDF 5.x no longer implicitly exposes that struct, and
pinning an older core (`version: 2.0.14`) is rejected outright by ESPHome
2026.7.2 ("Only Arduino 3.0+ is supported"). No config-level fix exists.

Also blocked independently: the `github://pionizer/pionizer-axp192`
external component `#include`s the Arduino-only `Esp.h`, which isn't
available under the ESP-IDF-based build path external_components run
through — a maintained, ESP-IDF-safe alternative exists
(`eigger/espcomponents`'s `axp192` component, actively pushed as of
2026-07-28), but its config schema and init sequence differ from the
current one (no M5StickC-specific power-rail init routine that the
current comment says "must be present to initialize TFT power on") —
swapping it could change display power-on behavior on real hardware, so
I didn't do it without your sign-off.

**Options:** (a) fork/patch `M5Station` to add the missing
`#include "soc/gpio_struct.h"`, or (b) replace the TFT driver with a
different, maintained library, or (c) leave this device on its current
ESPHome version until one of the above happens.

### `plantWaterer.yaml` — M5StickC Plant Waterer (192.168.178.226)
Same root cause as above, second half: `github://martydingo/esphome-axp192`
also `#include`s `Esp.h`, unavailable under the current ESP-IDF build path.
**Options:** same as m5station's axp192 blocker above — fork/patch the
component, or evaluate switching to `eigger/espcomponents`'s `axp192`
(different schema, needs behavior verification), or leave on current
firmware.

## Not upgraded — not an ESPHome device

`loraWatch.yaml` documents pin connections for a LilyGo TTGO T-Wristband,
which is **STM32-based** (pin names like `PB10`, `PC13` are STM32 GPIO
port/pin notation, not ESP32/ESP8266). The file has no `esphome:` key and
was never a working ESPHome config — it's pinout notes only. Left as-is.

## Devices not attempted: `sadbat.yaml`

Compiles clean but was never reachable — per its own header comment,
"First flash MUST be over USB serial — the device is not running ESPHome
yet." Not a regression; just not deployed yet. No OTA push possible until
it's flashed once over serial.
