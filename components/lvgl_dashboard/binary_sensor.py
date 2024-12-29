import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    DEVICE_CLASS_PROBLEM,
)

_ns = cg.esphome_ns.namespace("lvgl_dashboard")
_cls = _ns.class_("DashboardComponent", binary_sensor.BinarySensorInitiallyOff, cg.Component)

CONF_COMPONENT = "component"

CODEOWNERS = ["@kvj"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    device_class=DEVICE_CLASS_PROBLEM,
).extend({
    cv.GenerateID(): cv.declare_id(_cls),
    cv.Required(CONF_COMPONENT): cv.use_id(cg.Component),
})

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(var.set_component(await cg.get_variable(config[CONF_COMPONENT])))
    await cg.register_component(var, config)
