# sadbat Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `sadbat.yaml`, a new ESPHome node for the SparkFun Thing Plus ESP32 WROOM +
Qwiic LED Stick (APA102C), exposing a single web-controlled RGB light whose brightness slider
drives a center-out "pairs" bargraph/dimmer effect, with no Home Assistant integration.

**Architecture:** A small local ESPHome external component (`components/sadbat_led/`) implements
a custom `light::LightOutput` that talks raw I2C to the Qwiic LED Stick (no native ESPHome
component exists for it). The main `sadbat.yaml` wires up the board (GPIO0 Qwiic-power enable,
I2C bus, wifi networks + AP/captive-portal fallback, web_server) and instantiates this light.

**Tech Stack:** ESPHome 2026.7.2 (via this repo's docker `aliases`), ESP32 Arduino framework,
board id `esp32thing_plus`, raw I2C (no external libraries).

## Global Constraints

- Board id is exactly `esp32thing_plus` (SparkFun Thing Plus ESP32 WROOM, USB-C).
- Qwiic I2C bus: SDA=GPIO21, SCL=GPIO22. Qwiic connector power is gated by GPIO0 — must be
  driven HIGH before the LED stick will ACK on the bus.
- Qwiic LED Stick default I2C address: `0x23`. Command bytes: single-LED color=`0x71`
  `[led(1-10),r,g,b]`, single-LED brightness=`0x76` `[led(1-10),brightness(0-31)]`, all-off=`0x78`
  (no payload). LEDs are 1-indexed on the wire.
- Fill order (center-out pairs): `(5,6)`, `(4,7)`, `(3,8)`, `(2,9)`, `(1,10)`.
- `band = floor(brightness_pct / 20)` clamped to `[0,5]` pairs fully lit (brightness reg `31`);
  the next pair (if `brightness_pct` isn't an exact multiple of 20) gets a partial brightness reg
  `round((brightness_pct % 20) / 20 * 31)`; everything beyond that is off (color 0,0,0).
- Off (light off, or brightness 0%) → all 10 LEDs off.
- `restore_mode: ALWAYS_OFF` — never restore last state on boot.
- No `api:`, no `mqtt:` — control only via `web_server:`, which must run with `local: true` so
  the UI works with no internet (needed for the AP/captive-portal fallback case).
- `wifi.networks:` lists home wifi first, then work wifi — same firmware works in both places.
  `ap:` + `captive_portal:` remain as the last-resort fallback.
- This repo's `.gitignore` is a flat ignore-all-then-whitelist (`*` then `!*.yaml`/`!*.md`/etc) —
  any new subdirectory is invisible to git unless explicitly un-ignored (see Task 1).
- Full design rationale/sourcing lives in `sadbat-design.md` at the repo root — refer to it for
  *why*, not just *what*.

---

### Task 1: Un-ignore the new `components/` directory in `.gitignore`

**Files:**
- Modify: `.gitignore`

**Interfaces:**
- Produces: a repo where `components/**` is trackable by git (verified directly, no code
  interface).

- [ ] **Step 1: Add the un-ignore rules**

Append to the end of `.gitignore`:

```gitignore

# Local ESPHome external_components (custom light platform for sadbat)
!/components
!/components/**
```

- [ ] **Step 2: Verify it actually works**

This repo's gitignore is a blanket `*` followed by selective `!pattern` un-ignores — a plain
`!*.py`/`!*.h` rule does **not** reach into a brand-new directory because the directory itself is
still matched by the `*` rule (this bit a docs/ folder earlier in this project — see
`sadbat-design.md`). Confirm the fix before relying on it:

```bash
cd ~/src/esphomeyaml
mkdir -p components/sadbat_led && touch components/sadbat_led/test.py
git check-ignore -v components/sadbat_led/test.py; echo "exit: $?"
rm -rf components
```

Expected: `git check-ignore` prints nothing and exits `1` (not ignored). If it still prints a
match, the rule ordering or path is wrong — fix before continuing.

- [ ] **Step 3: Commit**

```bash
git add .gitignore
git commit -m "Allow components/ directory for sadbat's local external_component"
```

---

### Task 2: Scaffold `sadbat.yaml` — board, framework, logger, OTA

**Files:**
- Create: `sadbat.yaml`

**Interfaces:**
- Produces: a compiling (but not-yet-networked, not-yet-lit) ESPHome config other tasks extend.

- [ ] **Step 1: Write the base file**

```yaml
# sadbat — Qwiic LED Stick (APA102C) bargraph/dimmer controller
#
# SparkFun Thing Plus ESP32 WROOM (USB-C) + SparkFun Qwiic LED Stick - APA102C, wired via the
# board's Qwiic connector. See sadbat-design.md for the full design rationale (fill algorithm,
# wiring gotchas, protocol details, sourcing).
#
# First flash MUST be over USB serial — the device is not running ESPHome yet, so there is no
# OTA to take over. After that, OTA works normally.
# Build:  cd ~/src/esphomeyaml && . aliases && compile sadbat.yaml
# Flash:  esp_flash sadbat

substitutions:
  devicename: sadbat
  friendly_name: Sadbat LED Bar

esphome:
  name: ${devicename}
  comment: "Qwiic LED Stick bargraph/dimmer, no Home Assistant"

esp32:
  board: esp32thing_plus
  framework:
    type: arduino

logger:
```

> **Note:** `ota:` is deliberately **not** added here even though every other node in this repo
> has it from the start — ESPHome's `ota.esphome` platform requires a `network` component to be
> present (confirmed by a real compile failure: `Component ota.esphome requires component
> network`), so it's added together with `wifi:` in Task 4 instead.

- [ ] **Step 2: Verify it compiles**

This repo's `aliases` functions assume bash word-splitting on `$DOCKER_ARGS`; under this shell's
default zsh semantics that produces `unknown shorthand flag` errors from a single
un-split argument, and even under bash, `-ti` fails outside a real terminal. Use the equivalent
direct docker invocation instead:

```bash
cd ~/src/esphomeyaml
docker run --rm -v "$PWD":/config esphome/esphome:2026.7.2 compile sadbat.yaml
```

Expected: ends with `INFO Successfully compiled program.`

- [ ] **Step 3: Commit**

```bash
git add sadbat.yaml
git commit -m "sadbat: scaffold base config (board, framework, logger, ota)"
```

---

### Task 3: Qwiic power enable (GPIO0) + I2C bus

**Files:**
- Modify: `sadbat.yaml`

**Interfaces:**
- Produces: `output` component `id: qwiic_power_enable` (GPIO0), `i2c` bus `id: qwiic_bus`
  (GPIO21/22) — consumed by Task 5's light platform via `i2c_id: qwiic_bus`.

- [ ] **Step 1: Add the GPIO0 power-enable output and boot action**

Add to `sadbat.yaml`, right after the `esphome:` block's existing keys (turn `esphome:` into a
block with `on_boot:`, and add a new top-level `output:` block):

```yaml
esphome:
  name: ${devicename}
  comment: "Qwiic LED Stick bargraph/dimmer, no Home Assistant"
  on_boot:
    then:
      - output.turn_on: qwiic_power_enable

# ===== Qwiic connector power =====
# The Qwiic connector's LDO is OFF by default and must be switched on via GPIO0 before anything
# on the bus (including the LED stick) will respond. GPIO0 doubles as the BOOT button, so
# pressing it in the field will momentarily cut power to the LEDs. See sadbat-design.md.
output:
  - platform: gpio
    pin: GPIO0
    id: qwiic_power_enable
```

- [ ] **Step 2: Add the I2C bus**

Add a new top-level block:

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22
  id: qwiic_bus
```

- [ ] **Step 3: Verify it compiles**

```bash
cd ~/src/esphomeyaml
docker run --rm -v "$PWD":/config esphome/esphome:2026.7.2 compile sadbat.yaml
```

(See the note in Task 2 Step 2 on why this repo's `compile()` alias doesn't work directly in a
non-interactive/non-bash shell.) Expected: ends with `INFO Successfully compiled program.`

- [ ] **Step 4: Commit**

```bash
git add sadbat.yaml
git commit -m "sadbat: enable Qwiic connector power (GPIO0) and add I2C bus"
```

---

### Task 4: Network — home + work wifi, AP fallback, captive portal, web_server

**Files:**
- Modify: `sadbat.yaml`
- Modify: `secrets.yaml` (not tracked by git — this repo's `.gitignore` explicitly excludes it)

**Interfaces:**
- Consumes: `!secret wifi_ssid` / `!secret wifi_password` / `!secret ota_password` (already in
  `secrets.yaml`, shared with the rest of the fleet).
- Produces: new secrets `work_wifi_ssid`, `work_wifi_password`, `sadbat_ap_password` that must
  exist in `secrets.yaml` before this compiles.

- [ ] **Step 1: Get real values for the new secrets**

This step needs input that isn't available in the repo or design spec: **ask the user for the
actual work wifi SSID and password** before writing them into `secrets.yaml`. Generate the AP
fallback password yourself (any random string ≥ 8 characters — ESPHome's minimum for an `ap:`
password) rather than asking, since it's just a local emergency-config credential.

- [ ] **Step 2: Add the new secrets**

Append to `secrets.yaml` (this file is git-ignored — real values stay local only):

```yaml
work_wifi_ssid: <value the user provided>
work_wifi_password: <value the user provided>
sadbat_ap_password: <random string, e.g. from `openssl rand -base64 12`>
```

- [ ] **Step 3: Add the network config to `sadbat.yaml`**

```yaml
# ===== Network =====
# Tries home wifi first, then work wifi (built/tested at home, deployed at work); falls back to
# its own AP + captive portal only if neither known network is in range.
wifi:
  networks:
    - ssid: !secret wifi_ssid
      password: !secret wifi_password
    - ssid: !secret work_wifi_ssid
      password: !secret work_wifi_password
  ap:
    ssid: "sadbat-fallback"
    password: !secret sadbat_ap_password

captive_portal:

web_server:
  port: 80
  local: true

ota:
  platform: esphome
  password: !secret ota_password
```

`ota:` moves here from Task 2 — it requires a `network` component, which now exists.
`local: true` inlines and serves the web UI's JS/CSS from flash instead of an internet CDN — this
matters here specifically because the AP-fallback case has no internet access at all, and the UI
would otherwise fail to load.

- [ ] **Step 4: Verify it compiles**

```bash
cd ~/src/esphomeyaml
docker run --rm -v "$PWD":/config esphome/esphome:2026.7.2 compile sadbat.yaml
```

Expected: ends with `INFO Successfully compiled program.` (This step will download/embed local
web_server assets on first compile — expect it to take noticeably longer than earlier compiles.)

- [ ] **Step 5: Commit**

Only `sadbat.yaml` — `secrets.yaml` is git-ignored and must never be committed.

```bash
git add sadbat.yaml
git commit -m "sadbat: add home+work wifi, AP/captive-portal fallback, local web_server"
```

---

### Task 5: `sadbat_led` external component — I2C-driven bargraph/dimmer light

**Files:**
- Create: `components/sadbat_led/__init__.py`
- Create: `components/sadbat_led/light.py`
- Create: `components/sadbat_led/sadbat_led.h`
- Create: `components/sadbat_led/sadbat_led.cpp`
- Modify: `sadbat.yaml`

**Interfaces:**
- Produces: `light: platform: sadbat_led` usable in `sadbat.yaml`, backed by C++ class
  `sadbat_led::SadbatLedOutput` (inherits `light::LightOutput` + `i2c::I2CDevice`).
- Consumes: `i2c_id:` (an existing `i2c` bus id, here `qwiic_bus` from Task 3), `address:`
  (defaults to `0x23`).

ESPHome removed inline "custom component" lambdas in 2025 — the supported way to get a light
whose `write_state()` sees the full on/off + brightness + RGB state at once (needed here because
brightness drives the bargraph fill count, not just LED intensity) is a small real external
component, following the same shape as ESPHome's own built-in `rgb` light platform
(`esphome/components/rgb/`), just backed by I2C instead of PWM outputs.

- [ ] **Step 1: Create the empty package marker**

`components/sadbat_led/__init__.py` — empty file (matches the pattern used by ESPHome's own
platform-only components, e.g. `esp32_rmt_led_strip/__init__.py`):

```python
```

- [ ] **Step 2: Write the Python codegen (`light.py`)**

```python
import esphome.codegen as cg
from esphome.components import i2c, light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID

DEPENDENCIES = ["i2c"]

sadbat_led_ns = cg.esphome_ns.namespace("sadbat_led")
SadbatLedOutput = sadbat_led_ns.class_("SadbatLedOutput", light.LightOutput, i2c.I2CDevice)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SadbatLedOutput),
    }
).extend(i2c.i2c_device_schema(0x23))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await i2c.register_i2c_device(var, config)
```

- [ ] **Step 3: Write the C++ header (`sadbat_led.h`)**

```cpp
#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/light/light_output.h"

namespace esphome::sadbat_led {

class SadbatLedOutput : public light::LightOutput, public i2c::I2CDevice {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  void write_state(light::LightState *state) override;
};

}  // namespace esphome::sadbat_led
```

- [ ] **Step 4: Write the C++ implementation (`sadbat_led.cpp`)**

```cpp
#include "sadbat_led.h"
#include <cmath>
#include <cstdint>

namespace esphome::sadbat_led {

static const uint8_t CMD_WRITE_SINGLE_LED_COLOR = 0x71;
static const uint8_t CMD_WRITE_SINGLE_LED_BRIGHTNESS = 0x76;
static const uint8_t CMD_WRITE_ALL_LED_OFF = 0x78;

// LEDs are 1-indexed on the wire. Pairs fill from the center out.
static const uint8_t PAIRS[5][2] = {{5, 6}, {4, 7}, {3, 8}, {2, 9}, {1, 10}};

void SadbatLedOutput::write_state(light::LightState *state) {
  auto &values = state->current_values;

  if (!values.is_on()) {
    uint8_t cmd = CMD_WRITE_ALL_LED_OFF;
    this->write(&cmd, 1);
    return;
  }

  uint8_t red = static_cast<uint8_t>(roundf(values.get_red() * 255.0f));
  uint8_t green = static_cast<uint8_t>(roundf(values.get_green() * 255.0f));
  uint8_t blue = static_cast<uint8_t>(roundf(values.get_blue() * 255.0f));

  float pct = values.get_brightness() * 100.0f;
  int band = static_cast<int>(pct / 20.0f);
  if (band > 5)
    band = 5;
  float within_band = (pct - band * 20.0f) / 20.0f;
  uint8_t partial_brightness = static_cast<uint8_t>(roundf(within_band * 31.0f));

  for (int i = 0; i < 5; i++) {
    bool fully_lit = i < band;
    bool partially_lit = (i == band) && (partial_brightness > 0);

    for (int j = 0; j < 2; j++) {
      uint8_t led = PAIRS[i][j];

      if (fully_lit || partially_lit) {
        uint8_t color_cmd[5] = {CMD_WRITE_SINGLE_LED_COLOR, led, red, green, blue};
        this->write(color_cmd, 5);

        uint8_t brightness = fully_lit ? uint8_t(31) : partial_brightness;
        uint8_t brightness_cmd[3] = {CMD_WRITE_SINGLE_LED_BRIGHTNESS, led, brightness};
        this->write(brightness_cmd, 3);
      } else {
        uint8_t off_cmd[5] = {CMD_WRITE_SINGLE_LED_COLOR, led, 0, 0, 0};
        this->write(off_cmd, 5);
      }
    }
  }
}

}  // namespace esphome::sadbat_led
```

- [ ] **Step 5: Wire the component and the light into `sadbat.yaml`**

```yaml
external_components:
  - source:
      type: local
      path: components

light:
  - platform: sadbat_led
    id: sadbat_light
    name: "${friendly_name}"
    i2c_id: qwiic_bus
    address: 0x23
    restore_mode: ALWAYS_OFF
```

- [ ] **Step 6: Verify it compiles**

```bash
cd ~/src/esphomeyaml
docker run --rm -v "$PWD":/config esphome/esphome:2026.7.2 compile sadbat.yaml
```

Expected: ends with `INFO Successfully compiled program.` This is the step most likely to surface
a real error (Python schema typo, a
missing include, a C++ signature mismatch) — read the compiler output carefully rather than
assuming success. Common failure modes to check for if it fails: `CONF_OUTPUT_ID` not imported,
`i2c.i2c_device_schema` called without a default address argument, or a missing
`namespace esphome::sadbat_led` closing brace.

- [ ] **Step 7: Commit**

```bash
git add components/ sadbat.yaml
git commit -m "sadbat: add sadbat_led external component (I2C bargraph/dimmer light)"
```

---

### Task 6: First flash and hardware verification

**Files:** none (hardware/manual step)

**Interfaces:** none — this task consumes the finished `sadbat.yaml` from Task 5 and produces a
running physical device.

- [ ] **Step 1: Confirm the board is still attached and identify its port**

```bash
ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null
```

Expect to see the same `/dev/cu.usbserial-02762AEC` (or similar) confirmed earlier this session.

- [ ] **Step 2: Compile and flash over USB (first flash — device isn't running ESPHome yet)**

```bash
cd ~/src/esphomeyaml && . aliases
compile sadbat.yaml
esp_flash sadbat
```

`esp_flash` auto-detects the serial port if there's exactly one candidate; pass it explicitly
(`esp_flash sadbat /dev/cu.usbserial-02762AEC`) if there's ambiguity. See the `esp_flash()`
function in `aliases` for why this repo flashes natively instead of via `docker run ... upload`
(Docker Desktop on macOS can't pass through USB serial).

- [ ] **Step 3: Watch the boot log over serial**

```bash
docker run -ti --rm -v "${PWD}":/config esphome/esphome:2026.7.2 logs sadbat.yaml --device /dev/cu.usbserial-02762AEC
```

(Substitute the actual port from Step 1 if different.) Look for successful I2C bus setup and no
repeated connection/reboot errors. If wifi can't join either network yet (e.g. work wifi
credentials aren't valid from this location), that's expected — you should instead see it start
its own `sadbat-fallback` AP.

- [ ] **Step 4: Confirm network access**

- If it joined a known network: find its IP from the boot log, or try `http://sadbat.local/`.
- If it fell back to its own AP: on your phone/laptop, join the `sadbat-fallback` wifi network
  (password is whatever was generated into `secrets.yaml` in Task 4), then browse to
  `http://192.168.4.1/` — the captive portal should prompt automatically on most devices.

- [ ] **Step 5: Confirm the LEDs — this is the point to check the physical hardware**

Once the web UI loads (either path above), you'll see one entity: the light, with an on/off
toggle, a brightness slider, and a color wheel. Work through this sequence on the physical Qwiic
LED Stick and confirm each matches:

1. Toggle it **on** with default color/brightness — some center LEDs should light.
2. Set brightness to **~10%** — only the center pair (LEDs 5 and 6) should be lit, dim.
3. Raise brightness to **50%** — the center pair plus the next one out should be at full
   brightness (LEDs 4,5,6,7), with the third pair (3,8) partially lit.
4. Raise brightness to **100%** — all 10 LEDs lit at full brightness.
5. Change the color wheel (e.g. to blue) — currently-lit LEDs should shift color immediately.
6. Toggle it **off** — all 10 LEDs go fully dark.

If any step doesn't match, note exactly which step and what you saw (e.g. "at 50% only 2 LEDs are
lit, not 4") — that pinpoints whether the bug is in the band/partial-brightness math or the pair
mapping.

- [ ] **Step 6: Commit is not needed for this task** (no files changed) — instead, report back
  hardware verification results so any mismatch can be debugged against the algorithm in Task 5.
