import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RULER,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIMETER,
    UNIT_PERCENT,
)

from . import magnus_hawki_ns, MagnusHawki, CONF_MAGNUS_HAWKI_ID

DEPENDENCIES = ["magnus_hawki"]

CONF_DISTANCE = "distance"
CONF_LEVEL = "level"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAGNUS_HAWKI_ID): cv.use_id(MagnusHawki),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER,
            icon=ICON_RULER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:gauge",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_BATTERY,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MAGNUS_HAWKI_ID])

    if distance_config := config.get(CONF_DISTANCE):
        sens = await sensor.new_sensor(distance_config)
        cg.add(parent.set_distance_sensor(sens))

    if level_config := config.get(CONF_LEVEL):
        sens = await sensor.new_sensor(level_config)
        cg.add(parent.set_level_sensor(sens))

    if battery_config := config.get(CONF_BATTERY_LEVEL):
        sens = await sensor.new_sensor(battery_config)
        cg.add(parent.set_battery_sensor(sens))
