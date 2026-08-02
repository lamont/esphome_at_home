#pragma once

#include <array>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::amg8833 {

static const uint8_t AMG8833_PIXEL_COUNT = 64;

// One complete readout of the sensor. Kept as a plain value type so a consumer
// can take a consistent copy of the whole frame in one lock.
struct GridFrame {
  std::array<float, AMG8833_PIXEL_COUNT> pixels;
  float min_temp;
  float max_temp;
  float avg_temp;
  float device_temp;
  uint8_t min_index;
  uint8_t max_index;
  bool valid;
};

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
  void set_pixel_sensor(uint8_t index, sensor::Sensor *s) { pixel_sensors_[index] = s; }

  // Copies the most recent frame out under the frame lock. Safe to call from
  // another task (web handlers run in the async server task, not the main
  // loop), which is the whole reason the lock exists. Returns false when no
  // successful readout has happened yet.
  bool get_frame(GridFrame &out);

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
  std::array<sensor::Sensor *, AMG8833_PIXEL_COUNT> pixel_sensors_{};

  GridFrame frame_{};
  Mutex frame_lock_;

  bool setup_failed_{false};
};

}  // namespace esphome::amg8833
