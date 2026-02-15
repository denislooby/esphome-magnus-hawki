import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID

from . import magnus_hawki_ns, MagnusHawki, CONF_MAGNUS_HAWKI_ID

DEPENDENCIES = ["magnus_hawki"]

CONF_MEASURE = "measure"

MagnusHawkiMeasureButton = magnus_hawki_ns.class_(
    "MagnusHawkiMeasureButton",
    button.Button,
    cg.Parented.template(MagnusHawki),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAGNUS_HAWKI_ID): cv.use_id(MagnusHawki),
        cv.Optional(CONF_MEASURE): button.button_schema(
            MagnusHawkiMeasureButton,
            icon="mdi:radar",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MAGNUS_HAWKI_ID])

    if measure_config := config.get(CONF_MEASURE):
        btn = await button.new_button(measure_config)
        await cg.register_parented(btn, config[CONF_MAGNUS_HAWKI_ID])
