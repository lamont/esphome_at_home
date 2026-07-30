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
