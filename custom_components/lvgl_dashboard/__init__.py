from __future__ import annotations
from .constants import (
    DOMAIN, 
    PLATFORMS,
    FE_SCRIPT_URI,
)
from .coordinator import Coordinator
from .frontend import locate_dir

from homeassistant.core import HomeAssistant
from homeassistant.loader import async_get_integration
from homeassistant.helpers.typing import ConfigType

from homeassistant.helpers import service, reload
from homeassistant.const import SERVICE_RELOAD


from homeassistant.components import frontend, websocket_api, http
from aiohttp import web
from http import HTTPStatus

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

def _register_coordinator_service(hass: HomeAssistant,  name: str, handler):
    async def handler_(call):
        for entry_id in await service.async_extract_config_entry_ids(hass, call):
            if entry := hass.config_entries.async_get_entry(entry_id):
                _LOGGER.debug(f"_register_coordinator_service: {name}: {entry.domain}")
                if hasattr(entry, "runtime_data") and entry.domain == DOMAIN:
                    handler(entry.runtime_data, call.data)
    hass.services.async_register(DOMAIN, name, handler_)


class BrowserImageView(http.HomeAssistantView):
    """View to return a manifest.json."""

    requires_auth = False
    url = "/api/lvgl_dashboard/image/{entry_id}/{entity_id}/{scale}"
    name = "api:lvgl_dashboard:browser:image"

    def __init__(self, hass: HomeAssistant):
        self.hass = hass

    async def get(self, request: web.Request, entry_id: str, entity_id: str, scale: str) -> web.Response:
        _LOGGER.debug(f"BrowserImageView::get: request: {entry_id}, {entity_id}, {scale}")
        if c := _coordinator_by_msg(self.hass, {"config_entry_id": entry_id}):
            data, content_type = await c.async_picture_by_entity_id_scaled(entity_id, int(scale, 10))
            if data and content_type:
                _LOGGER.debug(f"BrowserImageView::get: response with image: {content_type}, {len(data)}")
                response = web.Response(
                    status=HTTPStatus.OK,
                    body=data,
                    content_type=content_type
                )
                response.enable_compression()
                return response
            else:
                _LOGGER.warning(f"BrowserImageView::get: failed to get image for {entity_id} and {scale}")
        else:
            _LOGGER.warning(f"BrowserImageView::get: invalid entry_id {entry_id}")
        return web.Response(status=HTTPStatus.NOT_FOUND)


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    cmp = await async_get_integration(hass, DOMAIN)
    ver = cmp.manifest["version"]

    conf = config.get(DOMAIN, {})
    _LOGGER.debug(f"async_setup: {ver}, {conf}")
    hass.data[DOMAIN] = conf
    async def _async_reload_yaml(call):
        config = await reload.async_integration_yaml_config(hass, DOMAIN)
        conf = config.get(DOMAIN, {})
        _LOGGER.debug(f"_async_reload_yaml: {conf}")
        hass.data[DOMAIN] = conf
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

    await hass.http.async_register_static_paths([
        http.StaticPathConfig(FE_SCRIPT_URI, "{}/lvgl-dashboard.js".format(locate_dir()), False),
    ])
    for fn in (_ws_event,):
        websocket_api.async_register_command(hass, fn)
    hass.http.register_view(BrowserImageView(hass))
    frontend.add_extra_js_url(hass, "{}?ver={}".format(FE_SCRIPT_URI, ver))

    return True

def _coordinator_by_msg(hass, msg: dict) -> Coordinator | None:
    if entry := hass.config_entries.async_get_entry(msg.get("config_entry_id")):
        _LOGGER.debug(f"_coordinator_by_msg: entry: {entry}")
        if entry.domain == DOMAIN and entry.runtime_data:
            return entry.runtime_data
    return None

WS_BASE_SCHEMA = {
    vol.Optional("device_id"): str,
    vol.Optional("config_entry_id"): str,
}

@websocket_api.websocket_command({
    **WS_BASE_SCHEMA,
    vol.Required("type"): "lvgl_dashboard/event",
    vol.Required("event"): str,
    vol.Optional("data"): dict,
})
@websocket_api.async_response
async def _ws_event(hass, connection, msg: dict):
    _LOGGER.debug("_ws_event: %s", msg)
    if c := _coordinator_by_msg(hass, msg):
        await c.async_handle_web_event(msg["event"], msg.get("data"))
    connection.send_result(msg["id"], {})
