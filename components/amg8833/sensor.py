import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from . import AMG8833Component

DEPENDENCIES = ["amg8833"]

CONF_AMG8833_ID = "amg8833_id"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_MIN_TEMPERATURE = "min_temperature"
CONF_AVG_TEMPERATURE = "avg_temperature"
CONF_MAX_PIXEL_INDEX = "max_pixel_index"
CONF_MIN_PIXEL_INDEX = "min_pixel_index"

TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

INDEX_SCHEMA = sensor.sensor_schema(
    icon="mdi:grid",
    accuracy_decimals=0,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AMG8833_ID): cv.use_id(AMG8833Component),
        cv.Optional(CONF_TEMPERATURE): TEMPERATURE_SCHEMA,
        cv.Optional(CONF_MAX_TEMPERATURE): TEMPERATURE_SCHEMA,
        cv.Optional(CONF_MIN_TEMPERATURE): TEMPERATURE_SCHEMA,
        cv.Optional(CONF_AVG_TEMPERATURE): TEMPERATURE_SCHEMA,
        cv.Optional(CONF_MAX_PIXEL_INDEX): INDEX_SCHEMA,
        cv.Optional(CONF_MIN_PIXEL_INDEX): INDEX_SCHEMA,
    }
)

SETTERS = {
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_MAX_TEMPERATURE: "set_max_temperature_sensor",
    CONF_MIN_TEMPERATURE: "set_min_temperature_sensor",
    CONF_AVG_TEMPERATURE: "set_avg_temperature_sensor",
    CONF_MAX_PIXEL_INDEX: "set_max_pixel_index_sensor",
    CONF_MIN_PIXEL_INDEX: "set_min_pixel_index_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_AMG8833_ID])
    for key, setter in SETTERS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(hub, setter)(sens))
