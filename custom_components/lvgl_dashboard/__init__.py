from __future__ import annotations
from .constants import DOMAIN, PLATFORMS
from .coordinator import Coordinator

from homeassistant.core import HomeAssistant
from homeassistant.helpers.typing import ConfigType

from homeassistant.helpers import service, reload
from homeassistant.const import SERVICE_RELOAD

import voluptuous as vol
import logging

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = vol.Schema({
    DOMAIN: vol.Schema({
    }, extra=vol.ALLOW_EXTRA),
}, extra=vol.ALLOW_EXTRA)

async def _async_update_entry(hass, entry):
    _LOGGER.debug(f"_async_update_entry: {entry}")
    coordinator = entry.runtime_data
    coordinator.load_options()
    await coordinator.async_send_dashboard()

async def async_setup_entry(hass: HomeAssistant, entry):

    coordinator = Coordinator(hass, entry)
    entry.runtime_data = coordinator

    entry.async_on_unload(entry.add_update_listener(_async_update_entry))
    await coordinator.async_config_entry_first_refresh()
    await coordinator.async_load()
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True

async def async_unload_entry(hass: HomeAssistant, entry):
    coordinator = entry.runtime_data

    await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    await coordinator.async_unload()
    entry.runtime_data = None
    return True

def get_dashboards(hass: HomeAssistant) -> dict:
    result = {"default": "Default"}
    for key, value in hass.data[DOMAIN].items():
        result[key] = value.get("title", key)
    return result

async def _async_reload_entries(hass: HomeAssistant):
    entries = hass.config_entries.async_entries(DOMAIN, False, False)
    for entry in entries:
        if coordinator := entry.runtime_data:
            await coordinator.async_send_dashboard()

def _register_coordinator_service(hass: HomeAssistant,  name: str, handler):
    async def handler_(call):
        for entry_id in await service.async_extract_config_entry_ids(hass, call):
            if entry := hass.config_entries.async_get_entry(entry_id):
                _LOGGER.debug(f"_register_coordinator_service: {name}: {entry.domain}")
                if hasattr(entry, "runtime_data") and entry.domain == DOMAIN:
                    handler(entry.runtime_data, call.data)
    hass.services.async_register(DOMAIN, name, handler_)


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    conf = config.get(DOMAIN, {})
    _LOGGER.debug(f"async_setup: {conf}")
    hass.data[DOMAIN] = conf
    async def _async_reload_yaml(call):
        config = await reload.async_integration_yaml_config(hass, DOMAIN)
        conf = config.get(DOMAIN, {})
        _LOGGER.debug(f"_async_reload_yaml: {conf}")
        hass.data[DOMAIN] = conf
        await _async_reload_entries(hass)
    service.async_register_admin_service(hass, DOMAIN, SERVICE_RELOAD, _async_reload_yaml)
    _register_coordinator_service(
        hass, "show_page", 
        lambda coord, data: hass.async_create_task(coord.async_send_show_page(data["page"]))
    )
    _register_coordinator_service(
        hass, "show_more_page", 
        lambda coord, data: hass.async_create_task(coord.async_send_show_more_page(data["entity"]))
    )
    _register_coordinator_service(
        hass, "hide_more_page", 
        lambda coord, data: hass.async_create_task(coord.async_send_hide_more_page())
    )
    _register_coordinator_service(
        hass, "play_sound", 
        lambda coord, data: hass.async_create_task(coord.async_send_play_rtttl(data["sound"]))
    )
    return True
