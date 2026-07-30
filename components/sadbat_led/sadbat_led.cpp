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
