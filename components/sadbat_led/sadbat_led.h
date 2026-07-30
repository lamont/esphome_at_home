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
