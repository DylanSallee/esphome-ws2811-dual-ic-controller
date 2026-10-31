"""Light platform for strips with two WS2811 ICs per pixel, one for RGB and one for white."""

import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_COLOR_CORRECT,
    CONF_CONSTANT_BRIGHTNESS,
    CONF_OUTPUT_ID,
    CONF_POWER_SUPPLY,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)
from esphome.types import ConfigType

CODEOWNERS = ["@DylanSallee"]

CONF_BRIGHTNESS_GAMMA = "brightness_gamma"
CONF_POWER_ESTIMATION = "power_estimation"
CONF_POWER_LIMIT = "power_limit"
CONF_STRIP_ID = "strip_id"
CONF_WHITE_MIX = "white_mix"
CONF_WHITE_STRENGTH = "white_strength"
CONF_WIRE_ORDER = "wire_order"

ws2811_dual_ic_ns = cg.esphome_ns.namespace("ws2811_dual_ic")
WS2811DualICLight = ws2811_dual_ic_ns.class_(
    "WS2811DualICLight", light.AddressableLight
)


def validate_wire_order(value: str) -> str:
    """Check the byte order of one pixel, e.g. RGBWCX. C and W are the white LEDs."""
    value = cv.string_strict(value).upper()
    if sorted(value) != sorted("RGBCWX"):
        raise cv.Invalid(
            f"'{value}' is not a valid wire order. List R, G, B, C, W and X exactly "
            "once each, in the order the ICs expect them. The first three bytes go to "
            "the first IC of the pixel, and X marks the unused byte."
        )
    return value


def white_led_rgb(mireds: float) -> list[float]:
    """Return a white LED's light per unit luminance, as linear RGB in Home Assistant's color space.

    The chromaticity comes from the Kim et al. fit to the Planckian locus. The matrix is the one
    homeassistant.util.color uses to turn xy into RGB, so white LEDs and colors share one space.
    """
    kelvin = 1_000_000 / mireds
    k, k2, k3 = 1e3 / kelvin, (1e3 / kelvin) ** 2, (1e3 / kelvin) ** 3
    if kelvin <= 4000:
        x = -0.2661239 * k3 - 0.2343589 * k2 + 0.8776956 * k + 0.179910
    else:
        x = -3.0258469 * k3 + 2.1070379 * k2 + 0.2226347 * k + 0.240390
    if kelvin <= 2222:
        y = -1.1063814 * x**3 - 1.34811020 * x**2 + 2.18555832 * x - 0.20219683
    elif kelvin <= 4000:
        y = -0.9549476 * x**3 - 1.37418593 * x**2 + 2.09137015 * x - 0.16748867
    else:
        y = 3.0817580 * x**3 - 5.87338670 * x**2 + 3.75112997 * x - 0.37001483
    big_x, big_z = x / y, (1 - x - y) / y
    return [
        big_x * 1.656492 - 0.354851 - big_z * 0.255038,
        -big_x * 0.707196 + 1.655397 + big_z * 0.036152,
        big_x * 0.051713 - 0.121364 + big_z * 1.011530,
    ]


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WS2811DualICLight),
            cv.Required(CONF_STRIP_ID): cv.use_id(light.AddressableLightState),
            cv.Required(CONF_COLD_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Required(CONF_WARM_WHITE_COLOR_TEMPERATURE): cv.color_temperature,
            cv.Optional(CONF_WIRE_ORDER, default="RGBWCX"): validate_wire_order,
            cv.Optional(CONF_CONSTANT_BRIGHTNESS, default=True): cv.boolean,
            cv.Optional(CONF_BRIGHTNESS_GAMMA, default=2.2): cv.positive_float,
            cv.Optional(CONF_WHITE_MIX, default=True): cv.boolean,
            cv.Optional(CONF_WHITE_STRENGTH, default=1.0): cv.positive_not_null_float,
            cv.Optional(CONF_POWER_LIMIT, default="100%"): cv.All(
                cv.percentage, cv.Range(min=0.01)
            ),
            cv.Optional(CONF_POWER_SUPPLY): cv.invalid(
                "Set power_supply on the strip's light. This light does not know "
                "when the ICs are dark."
            ),
            cv.Optional(CONF_POWER_ESTIMATION): cv.invalid(
                "power_estimation reads the effect buffer, which holds no brightness "
                "or white here. Use power_limit, which sees all five channels."
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    light.validate_color_temperature_channels,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    strip = await cg.get_variable(config[CONF_STRIP_ID])
    cg.add(var.set_strip(strip))

    order = config[CONF_WIRE_ORDER]
    cg.add(var.set_wire_order(*(order.index(channel) for channel in "RGBCW")))
    cg.add(
        var.set_white_mireds(
            config[CONF_COLD_WHITE_COLOR_TEMPERATURE],
            config[CONF_WARM_WHITE_COLOR_TEMPERATURE],
        )
    )
    cg.add(
        var.set_white_leds(
            *white_led_rgb(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]),
            *white_led_rgb(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]),
        )
    )
    cg.add(var.set_white_mix(config[CONF_WHITE_MIX]))
    cg.add(var.set_white_strength(config[CONF_WHITE_STRENGTH]))
    cg.add(var.set_constant_brightness(config[CONF_CONSTANT_BRIGHTNESS]))
    cg.add(var.set_brightness_gamma(config[CONF_BRIGHTNESS_GAMMA]))
    cg.add(var.set_power_limit(config[CONF_POWER_LIMIT]))
    if (balance := config.get(CONF_COLOR_CORRECT)) is not None:
        red, green, blue, *white = balance
        cg.add(var.set_channel_balance(red, green, blue, white[0] if white else 1.0))
