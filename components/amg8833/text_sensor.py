import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import AMG8833Component

DEPENDENCIES = ["amg8833"]

CONF_AMG8833_ID = "amg8833_id"
CONF_PIXELS = "pixels"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AMG8833_ID): cv.use_id(AMG8833Component),
        cv.Optional(CONF_PIXELS): text_sensor.text_sensor_schema(
            icon="mdi:grid",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_AMG8833_ID])
    if CONF_PIXELS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PIXELS])
        cg.add(hub.set_pixels_text_sensor(sens))
