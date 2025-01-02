from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import lvgl, font, api, switch, rtttl

from esphome.const import (
    CONF_ID,
)

_ns = cg.esphome_ns.namespace("lvgl_dashboard")
_cls = _ns.class_("LvglDashboard", cg.Component)

CODEOWNERS = ["@kvj"]
DEPENDENCIES = ["lvgl", "font", "api", "switch", "rtttl"]
AUTO_LOAD = ["json", "image"]

MULTI_CONF = False

CONF_LVGL = "lvgl_id"
CONF_BUTTONS = "buttons"
CONF_WIDTH = "width"
CONF_HEIGHT = "height"
CONF_MDI_FONT_SMALL = "mdi_font_small"
CONF_MDI_FONT_LARGE = "mdi_font_large"
CONF_FONT_NORMAL = "normal_font"
CONF_FONT_LARGE = "large_font"
CONF_API = "api"
CONF_SWITCHES = "switches"
CONF_BACKLIGHT = "backlight"
CONF_DESIGN = "design"
CONF_RTTTL = "rtttl"
CONF_DASHBOARD_RESET = "dashboard_reset"
CONF_VERTICAL = "vertical"
CONF_COMPONENTS = "components"

DASHBOARD_RESET_DEF = 10

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(_cls),
        cv.GenerateID(CONF_LVGL): cv.use_id(lvgl.lvcode.LvglComponent),
        cv.GenerateID(CONF_API): cv.use_id(api.APIServer),
        cv.Optional(CONF_WIDTH, default=400): cv.positive_int,
        cv.Optional(CONF_HEIGHT, default=400): cv.positive_int,
        cv.Required(CONF_MDI_FONT_SMALL): cv.use_id(font.Font),
        cv.Required(CONF_MDI_FONT_LARGE): cv.use_id(font.Font),
        cv.Required(CONF_FONT_NORMAL): cv.use_id(font.Font),
        cv.Optional(CONF_FONT_LARGE): cv.use_id(font.Font),
        cv.Required(CONF_SWITCHES): cv.ensure_list(cv.use_id(switch.Switch)),
        cv.Optional(CONF_DESIGN): cv.Schema({}, extra=cv.ALLOW_EXTRA),
        cv.Optional(CONF_BACKLIGHT): cv.use_id(switch.Switch),
        cv.Optional(CONF_RTTTL): cv.use_id(rtttl.Rtttl),
        cv.Optional(CONF_VERTICAL, default=False): cv.boolean,
        cv.Optional(CONF_DASHBOARD_RESET, default=DASHBOARD_RESET_DEF): cv.positive_int,
        cv.Optional(CONF_COMPONENTS, default=[]): cv.ensure_list(cv.use_id(cg.Component)),
    })
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    lvgl.defines.add_define("LV_USE_GRID")
    lvgl.defines.add_define("LV_USE_FLEX")
    lvgl.defines.add_define("LV_USE_BTN")
    lvgl.defines.add_define("LV_USE_LINE")
    lvgl.defines.add_define("LV_USE_LABEL")
    lvgl.defines.add_define("LV_USE_IMG")
    lvgl.defines.add_define("LV_USE_SWITCH")
    lvgl.defines.add_define("LV_USE_SLIDER")
    lvgl.defines.add_define("LV_USE_BAR")
    lvgl.defines.add_define("LV_USE_INDEV")
    lvgl.defines.add_define("USE_LVGL_FONT")
    lvgl.defines.add_define("LV_FONT_MONTSERRAT_12")
    lvgl.defines.add_define("LV_FONT_MONTSERRAT_28")
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_vertical(config[CONF_VERTICAL]))
    cg.add(var.set_config(config[CONF_WIDTH], config[CONF_HEIGHT]))
    cg.add(var.set_lvgl(await cg.get_variable(config[CONF_LVGL])))
    cg.add(var.set_api_server(await cg.get_variable(config[CONF_API])))
    cg.add(var.set_mdi_fonts(await cg.get_variable(config[CONF_MDI_FONT_SMALL]), await cg.get_variable(config[CONF_MDI_FONT_LARGE])))
    large_font_ = 0
    if CONF_FONT_LARGE in config:
        large_font_ = await cg.get_variable(config[CONF_FONT_LARGE])
    # else:
    #     lvgl.defines.add_define("LV_FONT_MONTSERRAT_28")
    cg.add(var.set_fonts(await cg.get_variable(config[CONF_FONT_NORMAL]), large_font_))
    for item in config[CONF_SWITCHES]:
        cg.add(var.add_switch(await cg.get_variable(item)))
    if CONF_BACKLIGHT in config:
        cg.add(var.set_backlight(await cg.get_variable(config[CONF_BACKLIGHT])))
    if CONF_RTTTL in config:
        cg.add(var.set_rtttl(await cg.get_variable(config[CONF_RTTTL])))
    cg.add(var.set_dashboard_reset_timeout(config[CONF_DASHBOARD_RESET]))
    if CONF_DESIGN in config:
        for key, value in config[CONF_DESIGN].items():
            cg.add_define(f"LVD_{key.upper()}", cg.RawExpression(value))
    for cmp in config[CONF_COMPONENTS]:
        cg.add(var.add_component(await cg.get_variable(cmp)))
    await cg.register_component(var, config)
