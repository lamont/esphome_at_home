#include "amg8833.h"
#include "esphome/core/log.h"
#include "esphome/core/alloc_helpers.h"

namespace esphome::amg8833 {

static const char *const TAG = "amg8833";

static const uint8_t REG_PCTL = 0x00;
static const uint8_t REG_RST = 0x01;
static const uint8_t REG_FPSC = 0x02;
static const uint8_t REG_TTHL = 0x0E;
static const uint8_t REG_PIXEL_OFFSET = 0x80;

static const uint8_t PCTL_NORMAL_MODE = 0x00;
static const uint8_t RST_INITIAL_RESET = 0x3F;
static const uint8_t FPSC_10FPS = 0x00;

static const float PIXEL_TEMP_LSB = 0.25f;
static const float THERMISTOR_TEMP_LSB = 0.0625f;

void AMG8833Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AMG8833...");
  if (!this->write_byte(REG_PCTL, PCTL_NORMAL_MODE)) {
    this->setup_failed_ = true;
    this->mark_failed();
    return;
  }
  this->write_byte(REG_RST, RST_INITIAL_RESET);
  delay(50);
  this->write_byte(REG_FPSC, FPSC_10FPS);
}

void AMG8833Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AMG8833:");
  LOG_I2C_DEVICE(this);
  if (this->setup_failed_) {
    ESP_LOGE(TAG, "  Communication with AMG8833 failed!");
  }
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Max Temperature", this->max_temperature_sensor_);
  LOG_SENSOR("  ", "Min Temperature", this->min_temperature_sensor_);
  LOG_SENSOR("  ", "Avg Temperature", this->avg_temperature_sensor_);
  LOG_SENSOR("  ", "Max Pixel Index", this->max_pixel_index_sensor_);
  LOG_SENSOR("  ", "Min Pixel Index", this->min_pixel_index_sensor_);
}

int16_t AMG8833Component::read_raw_(uint8_t reg) {
  uint8_t data[2];
  if (!this->read_bytes(reg, data, 2)) {
    return 0;
  }
  uint16_t magnitude_and_sign = (uint16_t(data[1] & 0x0F) << 8) | data[0];
  if (magnitude_and_sign & 0x800) {
    return -int16_t(magnitude_and_sign & 0x7FF);
  }
  return int16_t(magnitude_and_sign);
}

bool AMG8833Component::get_frame(GridFrame &out) {
  LockGuard guard(this->frame_lock_);
  out = this->frame_;
  return out.valid;
}

void AMG8833Component::update() {
  if (this->setup_failed_) {
    this->status_set_error();
    return;
  }

  GridFrame frame{};
  float min_temp = 0, max_temp = 0, avg_temp = 0;
  uint8_t min_index = 0, max_index = 0;
  std::string pixel_bytes;
  if (this->pixels_text_sensor_ != nullptr) {
    pixel_bytes.reserve(AMG8833_PIXEL_COUNT);
  }

  for (uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++) {
    int16_t raw = this->read_raw_(REG_PIXEL_OFFSET + i * 2);
    float temp = raw * PIXEL_TEMP_LSB;
    frame.pixels[i] = temp;
    if (i == 0 || temp > max_temp) {
      max_temp = temp;
      max_index = i;
    }
    if (i == 0 || temp < min_temp) {
      min_temp = temp;
      min_index = i;
    }
    avg_temp += temp;
    if (this->pixels_text_sensor_ != nullptr) {
      // One raw byte per pixel (low byte of the 12-bit register), matching
      // what the TheRealWaldo/thermal Home Assistant integration's
      // pixel_sensor decoder expects (base64 of 1 byte/pixel, value*0.25).
      pixel_bytes.push_back(static_cast<char>(raw & 0xFF));
    }
  }
  avg_temp /= AMG8833_PIXEL_COUNT;

  float device_temp = this->read_raw_(REG_TTHL) * THERMISTOR_TEMP_LSB;

  frame.min_temp = min_temp;
  frame.max_temp = max_temp;
  frame.avg_temp = avg_temp;
  frame.device_temp = device_temp;
  frame.min_index = min_index;
  frame.max_index = max_index;
  frame.valid = true;
  {
    LockGuard guard(this->frame_lock_);
    this->frame_ = frame;
  }

  for (uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++) {
    if (this->pixel_sensors_[i] != nullptr)
      this->pixel_sensors_[i]->publish_state(frame.pixels[i]);
  }

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(device_temp);
  if (this->max_temperature_sensor_ != nullptr)
    this->max_temperature_sensor_->publish_state(max_temp);
  if (this->min_temperature_sensor_ != nullptr)
    this->min_temperature_sensor_->publish_state(min_temp);
  if (this->avg_temperature_sensor_ != nullptr)
    this->avg_temperature_sensor_->publish_state(avg_temp);
  if (this->max_pixel_index_sensor_ != nullptr)
    this->max_pixel_index_sensor_->publish_state(max_index);
  if (this->min_pixel_index_sensor_ != nullptr)
    this->min_pixel_index_sensor_->publish_state(min_index);
  if (this->pixels_text_sensor_ != nullptr)
    this->pixels_text_sensor_->publish_state(
        base64_encode(reinterpret_cast<const uint8_t *>(pixel_bytes.data()), pixel_bytes.size()));

  this->status_clear_error();
}

}  // namespace esphome::amg8833
