#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::amg8833 {

static const uint8_t AMG8833_PIXEL_COUNT = 64;

// Native register-level Panasonic Grid-EYE AMG88xx driver, replacing the
// unmaintained SparkFun_GridEYE_Arduino_Library dependency the previous
// `custom:` platform hack used.
class AMG8833Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_temperature_sensor(sensor::Sensor *s) { temperature_sensor_ = s; }
  void set_max_temperature_sensor(sensor::Sensor *s) { max_temperature_sensor_ = s; }
  void set_min_temperature_sensor(sensor::Sensor *s) { min_temperature_sensor_ = s; }
  void set_avg_temperature_sensor(sensor::Sensor *s) { avg_temperature_sensor_ = s; }
  void set_max_pixel_index_sensor(sensor::Sensor *s) { max_pixel_index_sensor_ = s; }
  void set_min_pixel_index_sensor(sensor::Sensor *s) { min_pixel_index_sensor_ = s; }
  void set_pixels_text_sensor(text_sensor::TextSensor *s) { pixels_text_sensor_ = s; }

 protected:
  // Reads a 12-bit sign-magnitude register pair (pixel or thermistor
  // registers), per the AMG88xx datasheet: low byte + low nibble of high
  // byte form an 11-bit magnitude, top bit of that nibble is the sign.
  int16_t read_raw_(uint8_t reg);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *max_temperature_sensor_{nullptr};
  sensor::Sensor *min_temperature_sensor_{nullptr};
  sensor::Sensor *avg_temperature_sensor_{nullptr};
  sensor::Sensor *max_pixel_index_sensor_{nullptr};
  sensor::Sensor *min_pixel_index_sensor_{nullptr};
  text_sensor::TextSensor *pixels_text_sensor_{nullptr};

  bool setup_failed_{false};
};

}  // namespace esphome::amg8833
