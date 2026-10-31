"""A fake addressable strip for desktop tests. It records the bytes a real strip would be sent."""

import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID

capture_strip_ns = cg.esphome_ns.namespace("capture_strip")
CaptureStrip = capture_strip_ns.class_("CaptureStrip", light.AddressableLight)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(CaptureStrip),
        cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
