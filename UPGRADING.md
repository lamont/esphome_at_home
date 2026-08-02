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
| `m5station` | M5Stack Station — battery, SHT31, plant moisture/pump | ESP32 m5stack-station | Yes (192.168.178.221) | **No — blocked** (axp192 fixed 2026-08-02, unrelated M5Station lib issue remains) | No (left on old firmware) |
| `plantwaterer` (`plantWaterer.yaml`) | M5StickC plant moisture sensor + pump + display | ESP32 m5stick-c | Yes (192.168.178.226) | **Yes** (axp192 migrated 2026-08-02) | Not yet — pending sign-off |
| `sadbat` | Qwiic LED Stick bargraph/dimmer (no HA) | ESP32 thing_plus | No (never flashed OTA-capable yet — first flash is USB-only) | Yes | N/A (no OTA path) |
| `bedroomecho` | M5Stack Atom Echo — Home Assistant voice satellite | ESP32 m5stack-atom | No | Yes | N/A |
| `ble0` | Olimex PoE ESP32 — BLE proxy + iGrill listener | ESP32 esp32-poe (ethernet) | No | Yes | N/A |
| `co2sensor` | SCD41 CO2 + AMG8833 thermal testbed | ESP32-S3 um_feathers3 | No | Yes (AMG8833 rewritten 2026-08-02, see below) | N/A |
| `co2tdisplay` | CO2 monitor with ST7789 display, workroom | ESP32-S2 featheresp32-s2 | No | Yes | N/A |
| `feathers3um` | Feather S3 test platform | ESP32-S3 um_feathers3 | No | Yes | N/A |
| `indoorgrowupper` | Grow tent upper — AMG8833 leaf temp + SCD40 + BME280 | ESP32 esp32thing_plus | No | Yes | N/A |
| `litcontrol` | Boat multi-zone light controller | ESP32 featheresp32 | No | Yes | N/A |
| `thinggrideye` | ESP32 Thing Plus — AMG8833 testbed | ESP32 esp32thing_plus | No | Yes (AMG8833 rewritten 2026-08-02, see below) | N/A |
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
AMG8833 8x8 thermal camera testbed wiring used this pattern; initially
disabled (commented out) here, then rewritten as a proper native
external_component as follow-up work — see "AMG8833 rewrite" below.

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

### `m5station.yaml` — M5Stack Station (192.168.178.221)
The `m5stack/M5Station` Arduino library (unmaintained since 2022, only
ever released as v0.0.1 on the PlatformIO registry — no newer version
exists) does raw ESP32 GPIO register access
(`GPIO.out_w1ts = (1 << TFT_DC)`) in `src/utility/In_eSPI.h`. Current
arduino-esp32/ESP-IDF 5.x no longer implicitly exposes that struct, and
pinning an older core (`version: 2.0.14`) is rejected outright by ESPHome
2026.7.2 ("Only Arduino 3.0+ is supported"). No config-level fix exists.
This device's separate axp192 blocker (below) has been resolved, but this
one hasn't — the device is still on prior firmware pending an owner
decision.

**Options:** (a) fork/patch `M5Station` to add the missing
`#include "soc/gpio_struct.h"`, or (b) replace the TFT driver with a
different, maintained library, or (c) leave this device on its current
ESPHome version until one of the above happens.

## AMG8833 rewrite + axp192 migration (2026-08-02 follow-up)

### `co2sensor.yaml`, `thinggrideye.yaml` — AMG8833 support rewritten as a native external_component
Replaced the removed `custom:` platform hack with a proper local
external_component at `components/amg8833/` (a hub `amg8833:` component +
`sensor:`/`text_sensor:` platforms referencing it, matching the shape of
core hub components like `ads1115`). It's a from-scratch native I2C
register driver against the Panasonic Grid-EYE/AMG88xx datasheet — no
longer depends on the unmaintained `SparkFun_GridEYE_Arduino_Library`
Arduino library that had been pulled in via `libraries:`. Reproduces the
original behavior: `temperature`/`max_temperature`/`min_temperature`/
`avg_temperature`/`max_pixel_index`/`min_pixel_index` sensors, plus a
`pixels` text_sensor. The pixel payload deliberately keeps the original's
one-byte-per-pixel (not full 12-bit) base64 encoding — checked against the
`TheRealWaldo/thermal` Home Assistant integration's decoder
(`custom_components/thermal_vision/camera.py`, `_update_pixel_sensor`),
which expects exactly that format, since this text_sensor is meant to
feed that integration's thermal camera entity. Compiled clean for both
boards; neither device is currently reachable to OTA-flash and verify
against real hardware.

### `m5station.yaml`, `plantWaterer.yaml` — axp192 migrated to `eigger/espcomponents`
Both had been blocked on `Esp.h`-including axp192 forks incompatible with
the ESP-IDF build path (`pionizer/pionizer-axp192` and
`martydingo/esphome-axp192` respectively). Migrated both to
`github://eigger/espcomponents`'s `axp192` component, which is
ESP-IDF-safe and under active maintenance. Its schema changed from the
old `sensor: platform: axp192` (with a `model:`/`brightness:` config) to a
top-level `axp192:` hub domain with no `model:` key — its `setup()`
unconditionally runs the standard M5Stack LDO2/LDO3 TFT power-on sequence
(the same "must be present to initialize TFT power on" requirement the
old libraries handled via their `model:` parameter), so behavior should
be equivalent. `plantWaterer.yaml` now compiles clean end-to-end.
`m5station.yaml`'s axp192 half is fixed too (confirmed by the build error
moving cleanly past axp192 to the pre-existing, unrelated `M5Station`
library issue documented above), but the device as a whole is still
blocked by that separate issue.

## Known hardware issues found post-upgrade

### `snek.yaml` — Grove-bus sensors dead, needs physical inspection (2026-08-01)
All Grove-bus sensors (3x DHT temp/humidity + SHT3x over I2C) are reporting
failures: I2C scan finds no devices, the SHT3x component is marked FAILED,
and every DHT logs "Communication failed" → `nan`. This is not a config
issue — `snek` had been migrated from `switch: platform: gpio` (the pattern
every other `wio_link` device in the fleet still uses) to `output: platform:
gpio` with `mode: INPUT_PULLUP` on the GPIO15 power-gate pin, which looked
like a plausible regression. Reverted to the original `switch`-based pattern
and re-flashed via OTA to test; the identical failure persisted on the next
boot with the switch component and `on_boot: switch.turn_on` both verified
correct in the compiled config. Since two different, both previously-valid
pin-drive configs fail identically on the same hardware, **this needs a
physical inspection** — most likely a failed Grove power-gate transistor on
the Wio Link board, or a corroded/disconnected Grove hub connector (this
device lives in a snake enclosure, i.e. a humid environment). Left the
config on the simpler/fleet-consistent `switch` pattern regardless, since
it's a strict improvement even though it didn't fix the sensors.

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
