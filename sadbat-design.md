# sadbat — Qwiic LED Stick controller design

## Hardware

- **Board**: SparkFun Thing Plus ESP32 WROOM. ESPHome/PlatformIO board id
  `esp32thing_plus`. Confirmed attached via USB as a Silicon Labs CP2104 USB-UART
  bridge at `/dev/cu.usbserial-02762AEC` (serial `02762AEC`).
- **Peripheral**: SparkFun Qwiic LED Stick — APA102C (10 addressable LEDs, driven
  by an onboard ATtiny85 that bridges I2C to APA102 protocol). Connected via the
  board's Qwiic connector.
- **I2C bus**: SDA=GPIO23, SCL=GPIO22. **Note:** initial research (SparkFun's docs
  for what turned out to be the wrong board variant — see below) said SDA=GPIO21;
  the physical unit's own silkscreen, and an empirical I2C bus scan that only
  found the LED stick after switching pins, both say GPIO23. Trust the physical
  board over any doc page.
- **No Qwiic power switch on this board.** Earlier research (SparkFun doc pages,
  cross-checked twice) found that the newer USB-C Thing Plus variant gates Qwiic
  connector power through GPIO0 and an XC6222 LDO, and that finding got carried
  into this design on the assumption this board worked the same way — including
  an `on_boot` priority fix (ESPHome's `on_boot:` defaults to priority 600, after
  the `i2c:` bus's own priority-1000 startup scan, so a naive
  `on_boot: output.turn_on` looked like it was needed to make the scan see the
  device at all). **Both were wrong for this specific board.** The user directly
  inspected the schematic: Qwiic VCC here is hardwired straight to 3.3V, no
  switch. Re-examining the actual sequence of fixes confirms the SDA correction
  above is what actually made the I2C scan work — the GPIO0 toggling never did
  anything. All GPIO0 handling (the `output:` component, the `on_boot` action,
  and a later attempt to use it for a "power-cut on off" feature) was removed
  once this was confirmed. **Lesson:** a WebSearch summary of a product page is
  not the same evidence as reading that page directly, let alone reading the
  schematic — the weaker source should have been flagged as unverified before
  three separate pieces of firmware got built on top of it.

## Qwiic LED Stick I2C protocol

Default I2C address `0x23`. Command bytes (from SparkFun's own Arduino library
source, `SparkFun_Qwiic_LED_Stick_Arduino_Library`):

| Command | Byte | Payload |
|---|---|---|
| Write single LED color | `0x71` | `[led_number(1-10), r, g, b]` |
| Write all LEDs color | `0x72` | `[r, g, b]` |
| Write single LED brightness | `0x76` | `[led_number(1-10), brightness(0-31)]` |
| Write all LEDs brightness | `0x77` | `[brightness(0-31)]` |
| All LEDs off | `0x78` | (none) |

ESPHome has no built-in component for this device, and inline "custom component"
lambdas were removed from ESPHome in 2025 — this is driven by a small real
external_component (`components/sadbat_led/`) implementing `light::LightOutput` +
`i2c::I2CDevice` directly (same shape as ESPHome's own built-in `rgb` light
platform, just I2C instead of PWM outputs), not a lambda.

**I2C timing gotcha, confirmed on hardware:** the LED stick's onboard ATtiny85
bit-bangs the APA102 protocol out to the LEDs synchronously in response to each
I2C command, and its software I2C slave can't clock-stretch to make the ESP32
wait. Firing many writes back-to-back with no gap (e.g. one color + one
brightness write per LED, ×10 LEDs) desyncs it — observed as I2C timeout
warnings in the log and the LEDs getting stuck lit, not responding to further
commands until the stick's own power was cycled (an ESP32 reboot alone doesn't
reset it, since the GPIO0 power rail stays enabled across ESP32 reboots). Fixed
two ways: (1) a short `delay()` after every I2C write, empirically tuned to 2ms
against this one unit — not a datasheet value, since the ATtiny85 firmware
isn't published; (2) cut the transaction count roughly in half by writing color
once for all 10 LEDs (`CMD_WRITE_ALL_LED_COLOR`) instead of per-LED, since the
APA102 brightness register already zeroes an LED's output regardless of its
stored color — only brightness needs to vary per LED.

## Entity model

One ESPHome `light` (platform: `sadbat_led`, a local external_component) with
RGB + brightness support. This
is deliberately a single entity rather than a switch + number + color-picker
trio: ESPHome's `web_server` renders any `light` as an on/off toggle + brightness
slider + color wheel automatically, which is exactly "switch to turn all the way
off" + "configurable percent power" + "configurable color" — as one widget that
can't disagree with itself about state.

- **On/off** → the light's own state. Off means fully off (all-LEDs-off command
  issued), not just brightness 0, so the physical LEDs are dark and idle. This is
  a software-only off — the Qwiic connector's own power stays on either way,
  since (see Hardware above) there's no switch on this board to cut it.
- **Brightness (0-100%)** → drives the "pairs from the middle" bargraph fill
  (see algorithm below).
- **RGB color** → applied to whichever pairs are lit.
- **Boot/restore**: `restore_mode: ALWAYS_OFF`. The fixture always starts dark
  after a power cycle; a person must explicitly turn it on via the web UI.
- **`default_transition_length: 0s`** — confirmed on hardware that ESPHome's
  default 1s fade calls `write_state()` many times over the transition (worse
  when the web UI slider is dragged, which restarts the ramp with a new target
  before the old one finishes), which floods the LED stick's I2C slave faster
  than it can keep up even with the per-write delay below. A bargraph display
  doesn't need a fade, so transitions are disabled outright rather than tuned.

No `api:`, no `mqtt:`, no Home Assistant integration of any kind — control is
exclusively through ESPHome's built-in `web_server` UI.

## Fill algorithm

LEDs are numbered 1–10 on the wire protocol. Center-out pairs, in fill order:

1. `(5, 6)` — center
2. `(4, 7)`
3. `(3, 8)`
4. `(2, 9)`
5. `(1, 10)` — outer edge

Given `brightness_pct` (0–100) and `color (r,g,b)`:

- `band = floor(brightness_pct / 20)`, clamped to [0, 5] → number of *fully lit*
  pairs, filled from the center out.
- Any pair beyond `band` (not yet reached) is written to color (0,0,0) / off.
- The next pair after the fully-lit ones (if any remain and `brightness_pct` is
  not an exact multiple of 20) is written to the target color at a partial
  APA102 brightness register value: `round((brightness_pct % 20) / 20 * 31)`.
- Fully-lit pairs (within `band`) are written at full brightness register value
  `31`.
- At `brightness_pct == 100`, all 5 pairs are lit at brightness `31` in the
  chosen color.
- At `brightness_pct == 0` (or light off), all 10 LEDs are off.

## Network

Follows the existing fleet pattern (see `.home.yaml`, other nodes), extended with
a second known network since this fixture is built/tested at home but deployed
at work:

- `wifi.networks:` list with **two** known SSIDs — home (`!secret wifi_ssid` /
  `!secret wifi_password`, same as the rest of the fleet) and work (new
  `!secret work_wifi_ssid` / `!secret work_wifi_password`, to be added to
  `secrets.yaml`). ESPHome tries all listed networks and joins whichever is in
  range (strongest match), so the same firmware image works at home for testing
  and at work for deployment with no field reconfiguration.
- Still backed by an `ap:` fallback block (SSID `sadbat-fallback`, password from
  a new `!secret sadbat_ap_password` — min 8 chars per ESPHome's AP password
  requirement) and `captive_portal:`, as the true last resort if *neither* known
  network is in range (e.g. a third location, or the work SSID/password changes
  before the yaml is updated).
- `logger:` enabled.
- `ota:` password-protected (`!secret ota_password`, matching the rest of the
  fleet) — this repo's convention is first flash over USB, then OTA for all
  subsequent updates.
- `web_server:` port 80, no auth (matches `litcontrol.yaml` / `undercabinet.yaml`
  convention for other local-only fixtures in this repo).

## Out of scope

- No Home Assistant API/MQTT integration.
- No effects/animations beyond the static bargraph fill (no chase, rainbow,
  etc.) — can be added later as a `light` effect if wanted.
- No support for daisy-chaining a second Qwiic LED Stick (the protocol supports
  changing I2C address to allow this, but only one stick is attached).
- No hardware power-cut on "off" — there's no switch to do it with on this
  board (see Hardware above). "Off" is software-only (all-LEDs-off over I2C);
  idle power draw is whatever the LED stick pulls at rest, not zero.

## Future work (deliberately deferred)

- **Scheduled on/off** (e.g. turn on at a given time on Thursdays) — would need
  a `time:` component (e.g. `platform: sntp`) added, which nothing currently in
  this design requires. Deferred by the user as a later feature; not started.
