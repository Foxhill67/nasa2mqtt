import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, switch, select, number, climate
from esphome.const import (
    CONF_ID
)
from esphome.core import CORE

CODEOWNERS = ["foxhill67"]
DEPENDENCIES = ["uart"]
MULTI_CONF = False

CONF_NASA2MQTT_ID = "nasa2mqtt_id"

nasa2mqtt = cg.esphome_ns.namespace("nasa2mqtt")
NASA2MQTT = nasa2mqtt.class_(
    "NASA2MQTT", cg.PollingComponent, uart.UARTDevice
)

CONF_MQTT_HOST = "mqtt_host"
CONF_MQTT_PORT = "mqtt_port"
CONF_MQTT_USERNAME = "mqtt_username"
CONF_MQTT_PASSWORD = "mqtt_password"

CONF_DEBUG_LOG_MESSAGES = "debug_log_messages"
CONF_DEBUG_LOG_MESSAGES_RAW = "debug_log_messages_raw"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NASA2MQTT),
            cv.Optional(CONF_MQTT_HOST, default=""): cv.string,
            cv.Optional(CONF_MQTT_PORT, default=1883): cv.int_,
            cv.Optional(CONF_MQTT_USERNAME, default=""): cv.string,
            cv.Optional(CONF_MQTT_PASSWORD, default=""): cv.string,
            cv.Optional(CONF_DEBUG_LOG_MESSAGES, default=False): cv.boolean,
            cv.Optional(CONF_DEBUG_LOG_MESSAGES_RAW, default=False): cv.boolean
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("30s"))
)


async def to_code(config):
    # For MQTT
    if CORE.is_esp8266 or CORE.is_libretiny:
        cg.add_library("heman/AsyncMqttClient-esphome", "2.0.0")

    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_mqtt(config[CONF_MQTT_HOST], config[CONF_MQTT_PORT],
           config[CONF_MQTT_USERNAME], config[CONF_MQTT_PASSWORD]))

    if (CONF_DEBUG_LOG_MESSAGES in config):
        cg.add(var.set_debug_log_messages(config[CONF_DEBUG_LOG_MESSAGES]))

    if (CONF_DEBUG_LOG_MESSAGES_RAW in config):
        cg.add(var.set_debug_log_messages_raw(
            config[CONF_DEBUG_LOG_MESSAGES_RAW]))

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

