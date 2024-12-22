from collections.abc import Mapping
from typing import Any, cast

from homeassistant import config_entries
import homeassistant.helpers.config_validation as cv
from homeassistant.helpers.selector import selector

from homeassistant.const import (
    CONF_NAME,
    CONF_DEVICE_ID,
)

from homeassistant.helpers.schema_config_entry_flow import (
    SchemaConfigFlowHandler,
    SchemaFlowFormStep,
    SchemaFlowError,
)

from .constants import (
    DOMAIN,
    CONF_DASHBOARD,
    CONF_IS_BROWSER,
)

import voluptuous as vol
import logging

_LOGGER = logging.getLogger(__name__)

OPTIONS_SCHEMA = vol.Schema({
    vol.Optional(CONF_DASHBOARD): selector({"text": {}}),
})

CONFIG_SCHEMA = vol.Schema({
    vol.Required(CONF_NAME): selector({"text": {}}),
    vol.Required(CONF_IS_BROWSER, default=False): selector({"boolean": {}}),
    vol.Optional(CONF_DEVICE_ID): selector({"device": {"integration": "esphome"}}),
}).extend(OPTIONS_SCHEMA.schema)

async def _validate_options(step, user_input):
    _LOGGER.debug(f"_validate_options: {user_input}, {step}, {step.options}")
    return user_input

CONFIG_FLOW = {
    "user": SchemaFlowFormStep(CONFIG_SCHEMA, _validate_options),
}

OPTIONS_FLOW = {
    "init": SchemaFlowFormStep(OPTIONS_SCHEMA, _validate_options),
}

class ConfigFlowHandler(SchemaConfigFlowHandler, domain=DOMAIN):

    config_flow = CONFIG_FLOW
    options_flow = OPTIONS_FLOW

    def async_config_entry_title(self, options: Mapping[str, Any]) -> str:
        return cast(str, options[CONF_NAME])
