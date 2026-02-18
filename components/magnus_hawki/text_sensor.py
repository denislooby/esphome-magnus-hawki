import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from . import magnus_hawki_ns, MagnusHawki, CONF_MAGNUS_HAWKI_ID

DEPENDENCIES = ["magnus_hawki"]

CONF_LAST_READ = "last_read"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAGNUS_HAWKI_ID): cv.use_id(MagnusHawki),
        cv.Optional(CONF_LAST_READ): text_sensor.text_sensor_schema(
            icon="mdi:clock-outline",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MAGNUS_HAWKI_ID])

    if last_read_config := config.get(CONF_LAST_READ):
        sens = await text_sensor.new_text_sensor(last_read_config)
        cg.add(parent.set_last_read_sensor(sens))
