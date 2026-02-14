import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from . import magnus_hawki_ns, MagnusHawki, CONF_MAGNUS_HAWKI_ID

DEPENDENCIES = ["magnus_hawki"]

CONF_TIMESTAMP = "timestamp"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAGNUS_HAWKI_ID): cv.use_id(MagnusHawki),
        cv.Optional(CONF_TIMESTAMP): text_sensor.text_sensor_schema(
            icon="mdi:clock-outline",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MAGNUS_HAWKI_ID])

    if timestamp_config := config.get(CONF_TIMESTAMP):
        sens = await text_sensor.new_text_sensor(timestamp_config)
        cg.add(parent.set_timestamp_sensor(sens))
