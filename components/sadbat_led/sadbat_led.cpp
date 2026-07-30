#include "sadbat_led.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <cstdint>

namespace esphome::sadbat_led {

static const uint8_t CMD_WRITE_ALL_LED_COLOR = 0x72;
static const uint8_t CMD_WRITE_SINGLE_LED_BRIGHTNESS = 0x76;
static const uint8_t CMD_WRITE_ALL_LED_OFF = 0x78;

// The Qwiic LED Stick's onboard ATtiny85 bit-bangs the APA102 protocol out to the LEDs
// synchronously in response to each I2C command, and its software I2C slave can't clock-stretch
// to make the ESP32 wait. Back-to-back writes with no gap desync it (I2C timeouts, LEDs stuck),
// so every write is paced with a short delay to give it time to finish before the next command.
static const uint32_t COMMAND_DELAY_MS = 2;

// LEDs are 1-indexed on the wire. Pairs fill from the center out.
static const uint8_t PAIRS[5][2] = {{5, 6}, {4, 7}, {3, 8}, {2, 9}, {1, 10}};

void SadbatLedOutput::write_state(light::LightState *state) {
  auto &values = state->current_values;

  if (!values.is_on()) {
    uint8_t cmd = CMD_WRITE_ALL_LED_OFF;
    this->write(&cmd, 1);
    delay(COMMAND_DELAY_MS);
    return;
  }

  uint8_t red = static_cast<uint8_t>(roundf(values.get_red() * 255.0f));
  uint8_t green = static_cast<uint8_t>(roundf(values.get_green() * 255.0f));
  uint8_t blue = static_cast<uint8_t>(roundf(values.get_blue() * 255.0f));

  // The APA102 brightness register (0-31) already zeroes an LED's output regardless of its
  // stored color, so every LED can share one color write and only brightness needs to vary
  // per LED -- no need to separately blank the color of unlit LEDs.
  uint8_t color_cmd[4] = {CMD_WRITE_ALL_LED_COLOR, red, green, blue};
  this->write(color_cmd, 4);
  delay(COMMAND_DELAY_MS);

  float pct = values.get_brightness() * 100.0f;
  int band = static_cast<int>(pct / 20.0f);
  if (band > 5)
    band = 5;
  float within_band = (pct - band * 20.0f) / 20.0f;
  uint8_t partial_brightness = static_cast<uint8_t>(roundf(within_band * 31.0f));

  for (int i = 0; i < 5; i++) {
    bool fully_lit = i < band;
    bool partially_lit = (i == band) && (partial_brightness > 0);
    uint8_t brightness = fully_lit ? uint8_t(31) : (partially_lit ? partial_brightness : uint8_t(0));

    for (int j = 0; j < 2; j++) {
      uint8_t led = PAIRS[i][j];
      uint8_t brightness_cmd[3] = {CMD_WRITE_SINGLE_LED_BRIGHTNESS, led, brightness};
      this->write(brightness_cmd, 3);
      delay(COMMAND_DELAY_MS);
    }
  }
}

}  // namespace esphome::sadbat_led
