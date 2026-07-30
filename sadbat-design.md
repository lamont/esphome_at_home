# sadbat — Qwiic LED Stick controller design

## Hardware

- **Board**: SparkFun Thing Plus ESP32 WROOM (USB-C). ESPHome/PlatformIO board id
  `esp32thing_plus`. Confirmed attached via USB as a Silicon Labs CP2104 USB-UART
  bridge at `/dev/cu.usbserial-02762AEC` (serial `02762AEC`). Not yet flashed with
  ESPHome.
- **Peripheral**: SparkFun Qwiic LED Stick — APA102C (10 addressable LEDs, driven
  by an onboard ATtiny85 that bridges I2C to APA102 protocol). Connected via the
  board's Qwiic connector.
- **I2C bus**: SDA=GPIO21, SCL=GPIO22 (shared with the onboard MAX17048 fuel gauge,
  which is not used here).
- **Qwiic power gotcha**: the Qwiic connector's power (XC6222 LDO) is switched by
  **GPIO0**, which is LOW (off) by default and must be driven HIGH in firmware
  before the LED stick will respond on I2C. GPIO0 is also wired to the BOOT
  button, so pressing that button during normal operation will momentarily cut
  power to the LEDs. Verified against two independent SparkFun doc sources
  (`docs.sparkfun.com` hardware overview + `learn.sparkfun.com` hookup guide).

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

ESPHome has no built-in component for this device. It's driven with raw I2C
writes issued from a C++ lambda inside a template light's `write_state`, using
the `i2c::I2CBus`/`I2CDevice` write API directly — no external_component needed.

## Entity model

One ESPHome `light` (platform: `template`) with RGB + brightness support. This
is deliberately a single entity rather than a switch + number + color-picker
trio: ESPHome's `web_server` renders any `light` as an on/off toggle + brightness
slider + color wheel automatically, which is exactly "switch to turn all the way
off" + "configurable percent power" + "configurable color" — as one widget that
can't disagree with itself about state.

- **On/off** → the light's own state. Off means fully off (all-LEDs-off command
  issued), not just brightness 0, so the physical LEDs are dark and idle.
- **Brightness (0-100%)** → drives the "pairs from the middle" bargraph fill
  (see algorithm below).
- **RGB color** → applied to whichever pairs are lit.
- **Boot/restore**: `restore_mode: ALWAYS_OFF`. The fixture always starts dark
  after a power cycle; a person must explicitly turn it on via the web UI.

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
