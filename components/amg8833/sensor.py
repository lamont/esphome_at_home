import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_INTERNAL,
    CONF_MQTT_ID,
    CONF_NAME,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.core import ID

from . import AMG8833Component

DEPENDENCIES = ["amg8833"]

PIXEL_COUNT = 64

CONF_AMG8833_ID = "amg8833_id"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_MIN_TEMPERATURE = "min_temperature"
CONF_AVG_TEMPERATURE = "avg_temperature"
CONF_MAX_PIXEL_INDEX = "max_pixel_index"
CONF_MIN_PIXEL_INDEX = "min_pixel_index"
CONF_PIXELS = "pixels"

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

# One entity per pixel, expanded 64x at codegen. Internal by default: these are
# meant for /metrics (with `prometheus: include_internal: true`), not for
# cluttering Home Assistant or MQTT with 64 topics.
PIXELS_SCHEMA = TEMPERATURE_SCHEMA.extend(
    {
        cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
    }
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
        cv.Optional(CONF_PIXELS): PIXELS_SCHEMA,
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

    if CONF_PIXELS in config:
        base = config[CONF_PIXELS]
        for i in range(PIXEL_COUNT):
            # Clone the single validated config into one entity per pixel,
            # suffixing the name with the pixel index. Every declaration ID in
            # the clone has to be re-minted -- not just CONF_ID but also things
            # like mqtt_id, which would otherwise be registered 64 times.
            conf = dict(base)
            for key, value in list(conf.items()):
                if isinstance(value, ID) and value.is_declaration:
                    conf[key] = ID(
                        f"{value.id}_{i}", is_declaration=True, type=value.type
                    )
            if CONF_NAME in base:
                conf[CONF_NAME] = f"{base[CONF_NAME]} {i}"
            # Internal entities never publish to MQTT, so don't generate 64
            # MQTT component objects that would only burn RAM.
            if conf.get(CONF_INTERNAL) and CONF_MQTT_ID in conf:
                del conf[CONF_MQTT_ID]
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_pixel_sensor(i, sens))
