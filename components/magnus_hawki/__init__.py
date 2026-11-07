import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, esp32_ble_tracker
from esphome.const import CONF_ID

DEPENDENCIES = ["ble_client", "esp32_ble_tracker"]
CODEOWNERS = ["@denislooby"]
AUTO_LOAD = ["sensor", "text_sensor", "button"]

CONF_MAGNUS_HAWKI_ID = "magnus_hawki_id"
CONF_TANK_HEIGHT = "tank_height"
CONF_OFFSET = "offset"

magnus_hawki_ns = cg.esphome_ns.namespace("magnus_hawki")
MagnusHawki = magnus_hawki_ns.class_(
    "MagnusHawki",
    cg.PollingComponent,
    ble_client.BLEClientNode,
    esp32_ble_tracker.ESPBTDeviceListener,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MagnusHawki),
            cv.Optional(CONF_TANK_HEIGHT): cv.positive_float,
            cv.Optional(CONF_OFFSET, default=0): cv.float_,
        }
    )
    .extend(cv.polling_component_schema("14400s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    if CONF_TANK_HEIGHT in config:
        cg.add(var.set_tank_height(config[CONF_TANK_HEIGHT]))

    cg.add(var.set_offset(config[CONF_OFFSET]))
