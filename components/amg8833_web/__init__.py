import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID

from ..amg8833 import AMG8833Component, amg8833_ns

DEPENDENCIES = ["amg8833"]
AUTO_LOAD = ["web_server_base"]

CONF_AMG8833_ID = "amg8833_id"

AMG8833WebHandler = amg8833_ns.class_("AMG8833WebHandler", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AMG8833WebHandler),
        cv.GenerateID(CONF_AMG8833_ID): cv.use_id(AMG8833Component),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    hub = await cg.get_variable(config[CONF_AMG8833_ID])

    var = cg.new_Pvariable(config[CONF_ID], base, hub)
    await cg.register_component(var, config)
