from homeassistant.helpers.update_coordinator import (
    DataUpdateCoordinator,
    CoordinatorEntity,
)

from homeassistant.config_entries import (
    SIGNAL_CONFIG_ENTRY_CHANGED,
    ConfigEntryChange,
    ConfigEntryState,
)

from homeassistant.helpers import (
    dispatcher,
    event,
    network,
    template,
    device_registry,
)

from homeassistant.components import camera, image, light
from homeassistant.const import (
    CONF_DEVICE_ID,
    CONF_NAME,
)


from .constants import (
    DOMAIN,
    CONF_DASHBOARD,
    TOGGLABLE_DOMAINS,
    SLIDER_DOMAINS,
    IMAGE_DOMAINS,
)

from .mdi_font.icon import icon_from_state

from .mdi_font import GlyphProvider
from .picture import bytes_to_565_ints, async_get_image_by_entity_id

import collections.abc
import logging
import json, copy
from datetime import datetime

_LOGGER = logging.getLogger(__name__)

SET_DATA_BATCH = 500
PICTURE_DEF_SCALE_ITEM = 60
PICTURE_DEF_SCALE_MORE = 400

ICON_SMALL = 25
ICON_LARGE = 55

class Coordinator(DataUpdateCoordinator):

    def __init__(self, hass, entry):
        super().__init__(
            hass,
            _LOGGER,
            name=DOMAIN,
            setup_method=self._async_setup,
            update_method=self._async_update,
        )
        self._entry = entry
        self._entry_id = entry.entry_id

        self._dashboard = None

        self._on_config_entry_handler = None
        self._on_device_updated_handler = None
        self._on_service_call_handler = None
        self._entry_data = None
        self._on_entity_state_handler = None

    async def _async_setup(self):
        self._mdi_font = GlyphProvider()
        await self.hass.async_add_executor_job(self._mdi_font.init)

    async def _async_update(self):
        return {
            "page": 0,
            "backlight": False,
            "connected": False,
            "more_page": False,
        }

    async def _async_update_state(self, data: dict):
        self.async_set_updated_data({
            **self.data,
            **data,
        })

    def _pick_entity_ids(self, item: dict) -> set[str]:
        ids = set()
        if "entity_id" in item:
            ids.add(self._g(item, "entity_id"))
        if "entity_ids" in item:
            ids.update([ self._gv(id_) for id_ in self._g(item, "entity_ids") ])
        for item_ in self.all_items(item, "items"):
            if "entity_id" in item_:
                ids.add(self._g(item_, "entity_id"))
        return ids
    
    async def _async_on_state_change(self, entity_id: str, from_state, to_state):
        _LOGGER.debug(f"_async_on_state_change: {entity_id}")
        await self.async_send_values(entity_id)

    def _dashboard_items(self):
        if self._dashboard:
            page_no = 0
            for page in self.all_items(self._dashboard, "pages"):
                item_no = 0
                for item in self.all_items(page, "items"):
                    yield (page_no, page, item_no, item)
                    item_no += 1
                page_no += 1
    
    def _to_template(self, value: any):
        if isinstance(value, list):
            return [self._to_template(item) for item in value]
        if isinstance(value, collections.abc.Mapping):
            return {key: self._to_template(value_) for key, value_ in value.items()}
        if isinstance(value, str):
            return template.Template(value, self.hass)
        return value
    
    def _gv(self, value, def_=None, state=None):
        if isinstance(value, template.Template):
            try:
                return template.render_complex(value, {"this": state})
            except:
                _LOGGER.exception(f"_gv: value = {value}, def_ = {def_}, state = {state}")
                return def_
        return value

    def _g(self, obj: dict, key: str, def_=None, state=None):
        return self._gv(obj.get(key, def_), def_=def_, state=state)

    def name_from_state(self, entity_id: str | None, state) -> str:
        if state:
            return state.attributes.get("friendly_name", state.entity_id)
        return entity_id if entity_id else ""
    
    def api_url(self, path: str) -> str:
        url = network.get_url(self.hass, allow_ip=True, prefer_external=False)
        return f"{url}{path}?ts={datetime.now().timestamp()}"
    
    async def async_picture_from_state(self, entity_id: str | None, state, size: int):
        # return (None, None)
        domain, id = entity_id.split(".") if entity_id else ("", "")
        try:
            if domain == "camera":
                image_ = await camera.async_get_image(self.hass, entity_id, width=800, height=800)
                _LOGGER.debug(f"async_prepare_data: picture: {image_.content_type}, {len(image_.content)}")
                size, data = bytes_to_565_ints(image_.content, image_.content_type, size)
                return (size, data)
            if domain == "image":
                image_ = await async_get_image_by_entity_id(self.hass, entity_id)
                if image_:
                    size_, data_ = bytes_to_565_ints(image_.content, image_.content_type, size)
                    return (size_, data_)
        except:
            _LOGGER.exception(f"async_picture_from_state: error getting picture")
        return (None, None)
    
    def color_from_state(self, state, item: dict) -> str:
        result = ""
        if col_ := self._g(item, "color", state=state):
            if isinstance(col_, collections.abc.Mapping):
                if state and (state.state in col_):
                    return self._g(col_, state.state, state=state)
                return ""
            else:
                return col_
        if state and state.state == "on":
            result = "on"
        return result
    
    def state_by_entity_id(self, entity_id: str | None):
        return self.hass.states.get(entity_id) if entity_id else None

    async def async_send_picture_data(self, service: str, entity_id: str, scale: int, cb):
        state = self.state_by_entity_id(entity_id)
        size, data = await self.async_picture_from_state(entity_id, state, scale)
        if size and data:
            offset = 0
            while offset < len(data):
                self.call_device_service(service, {
                    "data": data[offset:(offset + SET_DATA_BATCH)], 
                    "offset": offset, "size": len(data),
                    **cb(),
                })
                offset += SET_DATA_BATCH

    async def async_prepare_data(self, layout: str, item: dict) -> list:
        entity_id = self._g(item, "entity_id")
        state = self.state_by_entity_id(entity_id)
        hidden = self._g(item, "hidden", state=state)
        if hidden:
            return {"_h": True}
        if layout == "button":
            icon = icon_from_state(self._g(item, "icon", state=state), state)
            icon_size = int(self._g(item, "size", ICON_LARGE, state=state))
            return {
                "name": self._g(item, "name", self.name_from_state(entity_id, state), state=state),
                "icon": self._mdi_font.get_icon_value(icon, icon_size),
                "ctype": self._g(item, "ctype", "button"),
                "col": self.color_from_state(state, item),
            }
        if layout == "sensor":
            icon = icon_from_state(self._g(item, "icon", state=state), state)
            icon_size = int(self._g(item, "size", ICON_SMALL, state=state))
            return {
                "name": self._g(item, "name", self.name_from_state(entity_id, state), state=state),
                "icon": self._mdi_font.get_icon_value(icon, icon_size),
                "value": str(self._g(item, "value", state.state if state else "", state=state)),
                "unit": self._g(item, "unit", state.attributes.get("unit_of_measurement", "") if state else ""),
                "ctype": self._g(item, "ctype", "text"),
                "col": self.color_from_state(state, item),
            }
        if layout == "picture":
            size, data = await self.async_picture_from_state(
                entity_id, state, 
                self._g(item, "scale", PICTURE_DEF_SCALE_ITEM, state=state)
            )
            if size and data:
                return {
                    "ctype": self._g(item, "ctype", "button"),
                    "col": self.color_from_state(state, item),
                    "image": {"width": size[0], "height": size[1]}
                }
        if layout == "layout":
            items = []
            for item_ in self.all_items(item, "items"):
                entity_id_ = self._g(item_, "entity_id", state=state)
                state_ = self.state_by_entity_id(entity_id_)
                item__ = {"ctype": self._g(item_, "ctype", "text", state=state_)}
                for key, name in (("col", "x"), ("row", "y"), ("cols", "w"), ("rows", "h")):
                    if key in item_:
                        item__[name] = self._g(item_, key, state=state_)
                if "label" in item_:
                    item__["label"] = self._g(item_, "label", state=state_)
                else:
                    item__["icon"] = self._mdi_font.get_icon_value(
                        icon_from_state(self._g(item_, "icon", state=state_), state_), 
                        self._g(item_, "size", ICON_SMALL, state=state_)
                    )
                item__["col"] = self.color_from_state(state_, item_)
                item__["_h"] = self._g(item_, "hidden", state=state_)
                items.append(item__)
            ctype = self._g(item, "ctype", "button")
            return {
                "ctype": ctype,
                "col": self.color_from_state(state, item),
                "items": items,
                "cols": self._g(item, "lc", [1], state=state),
                "rows": self._g(item, "lr", [1], state=state),
            }
        return None

    async def async_send_values(self, entity_id: str | None = None, page: int | None = None):
        for (page_no, _, item_no, item) in self._dashboard_items():
            if (entity_id is None or entity_id in self._pick_entity_ids(item)) and (page is None or page == page_no):
                if op := await self.async_prepare_data(self._g(item, "layout", "button"), item):
                    _LOGGER.debug(f"async_send_values: set_value: {page_no}, {item_no}, {op}")
                    self.call_device_service("set_value", {
                        "page": page_no, "item": item_no, "json_value": json.dumps(op)
                    })

    async def async_prepare_button(self, item: dict):
        result = {}
        for key in ("left", "right"):
            if key in item and "icon" in item[key]:
                result[key] = {
                    "icon": self._mdi_font.get_icon_value(icon_from_state(self._g(item[key], "icon"), None), 30),
                }
        return result
    
    def all_items(self, obj: dict, name: str, depth=3):
        name_ = name
        for _ in range(depth):
            for item in obj.get(name_, []):
                yield item
            name_ += "_"

    async def async_send_dashboard(self):
        self._on_entity_state_handler = self._disable_listener(self._on_entity_state_handler)
        name = self._config.get(CONF_DASHBOARD)
        if not name:
            name = "default"
        dashboard = self.get_dashboard(name)
        self._dashboard = self._to_template(dashboard)
        theme = {}
        for key in self._g(self._dashboard, "theme", {}):
            value = self._g(self._dashboard["theme"], key)
            if value or value == 0:
                theme[key] = str(value)
        _LOGGER.debug(f"async_send_dashboard: set_theme: {theme}")
        self.call_device_service("set_theme", {"json_value": json.dumps(theme)})
        data = []
        all_entity_ids = set()
        for page in self.all_items(self._dashboard, "pages"):
            page_data = {
                "rows": self._g(page, "rows", 4), 
                "cols": self._g(page, "cols", 4), 
                "items": []
            }
            for item in self.all_items(page, "items"):
                item_data = {"layout": self._g(item, "layout", "button")}
                for attr in ("col", "row", "cols", "rows"):
                    if attr in item:
                        item_data[attr] = self._g(item, attr)
                all_entity_ids.update(self._pick_entity_ids(item))
                page_data["items"].append(item_data)
            data.append(json.dumps(page_data))
        self.call_device_service("set_pages", {"jsons": data, "page": 0})
        idx = 0
        for button in self._dashboard.get("buttons", []):
            self.call_device_service("set_button", {
                "index": idx, 
                "json_value": json.dumps(await self.async_prepare_button(button)),
            })
            idx += 1
        self._on_entity_state_handler = event.async_track_state_change(
            self.hass, 
            all_entity_ids, 
            action=self._async_on_state_change
        )
        await self.async_send_values(None)

    async def async_send_show_more_page(self, entity_id: str, item: dict | None = None):
        state = self.state_by_entity_id(entity_id)
        features = []
        if not state:
            return
        [domain, name] = entity_id.split(".")
        if domain in TOGGLABLE_DOMAINS:
            features.append({"type": "toggle", "id": "toggle", "value": state.state == "on"})
        if domain == "light":
            if light.brightness_supported(light.get_supported_color_modes(self.hass, entity_id)):
                features.append({
                    "type": "slider", 
                    "min": 0, "max": 100, 
                    "id": "bri",
                    "value": state.attributes.get("brightness", 0),
                })
        if domain in SLIDER_DOMAINS:
            features.append({
                "type": "slider", 
                "min": state.attributes.get("min", 0), 
                "max": state.attributes.get("max", 100),
                "id": "value",
                "value": state.state,
            })
        if domain in IMAGE_DOMAINS:
            size, data = await self.async_picture_from_state(entity_id, state, PICTURE_DEF_SCALE_MORE)
            if size and data:
                features.append({
                    "type": "image", 
                    "width": size[0], 
                    "height": size[1],
                    "id": "image",
                })
        self.call_device_service("show_more", {
            "json_value": json.dumps({
                "features": features, "id": entity_id,
                "title": self.name_from_state(entity_id, state)
            })
        })

    def get_dashboard(self, name: str) -> dict:
        result = self.hass.data[DOMAIN].get(name)
        if result:
            return copy.deepcopy(result)
        return {
            "pages": [{"rows": 1, "cols": 1, "items": [{
                "layout": "button",
                "name": "No dashboard",
                "icon": "mdi:file-question",
                "icon_size": 100,
            }]}]
        }

    def call_device_service(self, name: str, data: dict) -> bool:
        if not self.is_device_connected():
            _LOGGER.debug(f"call_device_service: not connected")
            return False
        for _, service in self._entry_data.services.items():
            if service.name == name:
                # _LOGGER.debug(f"call_device_service: call service {name} with {data}, spec: {service}")
                self._entry_data.client.execute_service(service, data)
                return True
        return False

    def is_device_connected(self) -> bool:
        return True if self._entry_data and self._entry_data.available else False
    
    def _get_item_def(self, page: int, item: int) -> dict | None:
        pages = list(self.all_items(self._dashboard, "pages"))
        if 0 <= page < len(pages):
            items = list(self.all_items(pages[page], "items"))
            if 0 <= item < len(items):
                return items[item]
        return None
    
    async def async_exec_change_action(self, entity_id: str, op: str, value: int):
        action = None
        extra = {}
        if op == "toggle":
            action = "homeassistant.turn_on" if value == 1 else "homeassistant.turn_off"
        if op == "bri":
            action = "light.turn_on"
            extra = {"brightness_pct": value}
        if op == "value":
            [domain, name] = entity_id.split(".")
            action = f"{domain}.set_value"
            extra = {"value": value}
        if action:
            _LOGGER.debug(f"async_exec_change_action: action = {action}, entity = {entity_id}, extra={extra}")
            [domain, name] = action.split(".")
            await self.hass.services.async_call(domain, name, {
                "entity_id": entity_id,
                **extra,
            }, blocking=False)

        
    async def async_execute_hass_action(self, entity_id: str, action_def: dict):
        id = self._g(action_def, "entity_id", entity_id)
        [domain, name] = id.split(".")
        action = "homeassistant.toggle"
        if domain == "script":
            action = entity_id
        elif domain == "automation":
            action = "automation.trigger"
        elif domain == "scene":
            action = "scene.apply"
        elif domain == "button":
            action = "button.press"
        if "action" in action_def:
            action = action_def["action"]
        extra = self._g(action_def, "extra", {})
        
        _LOGGER.debug(f"async_execute_hass_action: action = {action}, entity = {id}, extra={extra}")
        [domain, name] = action.split(".")
        await self.hass.services.async_call(domain, name, {
            "entity_id": id,
            **extra,
        }, blocking=False)

    async def async_send_show_page(self, index: int):
        self.call_device_service("show_page", {"page": index})
        await self.async_send_values(page=index)

    async def async_send_hide_more_page(self):
        self.call_device_service("hide_more", {})
    
    async def async_send_play_rtttl(self, song: str):
        self.call_device_service("play_rtttl", {"song": song})

    async def async_exec_action(self, action: dict | None, item_def: dict) -> bool:
        if not action:
            return False
        if "page" in action:
            page_no = self._g(action, "page")
            await self.async_send_show_page(page_no)
        if "toggle" in action or "action" in action:
            await self.async_execute_hass_action(self._g(item_def, "entity_id"), action)
        if "more" in action:
            more = self._g(action, "more")
            entity_id = self._g(item_def, "entity_id") if more == True else more
            await self.async_send_show_more_page(entity_id, item_def)

    async def async_handle_event(self, type_: str, event: dict):
        page = int(event.get("page", -1))
        item = int(event.get("item", -1))
        entity_id = event.get("id")
        op = event.get("op")
        
        if type_ == "page":
            await self._async_update_state({"page": page})
        if type_ == "more":
            visible = event.get("visible") == "1"
            changed = self.data.get("more_page", False) != visible
            await self._async_update_state({"more_page": visible})
            if not visible and changed:
                await self.async_send_values(page=self.data.get("page", 0))
        if type_ in ("button", "long_button"):
            btns = self._dashboard.get("buttons", [])
            if 0 <= item < len(btns):
                btn = btns[item]
                await self.async_exec_action(self._g(btn, "on_tap" if type_ == "button" else "on_long_tap"), btn)
        if item_def := self._get_item_def(page, item):
            if type_ == "click":
                await self.async_exec_action(self._g(item_def, "on_tap"), item_def)
            if type_ == "long_press":
                await self.async_exec_action(self._g(item_def, "on_long_tap"), item_def)
            if type_ == "data_request":
                entity_id_ = self._g(item_def, "entity_id")
                scale = self._g(item_def, "scale", PICTURE_DEF_SCALE_ITEM)
                await self.async_send_picture_data("set_data", entity_id_, scale, lambda: {"item": item, "page": page})
        if entity_id and op:
            if type_ == "change":
                value = int(event.get("value", 0))
                await self.async_exec_change_action(entity_id, op, value)
            if type_ == "data_request":
                await self.async_send_picture_data("set_data_more", entity_id, PICTURE_DEF_SCALE_MORE, lambda: {})

    def _connect_to_services(self, entry_data):
        def _on_service_call(call):
            _LOGGER.debug(f"_on_service_call: {call}")
            if call.service == "esphome.lvgl_dashboard_event":
                type_ = call.data.get("type")
                self.hass.async_create_task(self.async_handle_event(type_, call.data))
        if not self._on_service_call_handler:
            self._on_service_call_handler = entry_data.client.subscribe_service_calls(_on_service_call)
            _LOGGER.debug(f"_connect_to_services: {entry_data.services}")
            self.hass.async_create_task(self.async_send_dashboard())

    def _disconnect_from_services(self):
        _LOGGER.debug(f"_disconnect_from_services: {self._on_service_call_handler}")
        self._on_service_call_handler = self._disable_listener(self._on_service_call_handler)
    
    def _connect_to_esphome_device(self, entry_data):
        def _on_device_update():
            _LOGGER.debug(f"_on_device_update: {entry_data.available}")
            if entry_data.available:
                self._connect_to_services(entry_data)
            else:
                self._disconnect_from_services()
            self.hass.async_create_task(self._async_update_state({"connected": self.is_device_connected()}))
            

        self._on_device_updated_handler = entry_data.async_subscribe_device_updated(_on_device_update)
        self._entry_data = entry_data
        _LOGGER.debug(f"_connect_to_esphome_device: {entry_data.available}")
        if entry_data.available:
            _on_device_update()

    def _disconnect_from_esphome(self):
        if self._entry_data:
            _LOGGER.debug(f"_disconnect_from_esphome: {self._on_device_updated_handler}")
            self._on_device_updated_handler = self._disable_listener(self._on_device_updated_handler)
        self._entry_data = None

    def load_options(self):
        self._config = self._entry.as_dict()["options"]

    def _config_entry_by_device_id(self, device_id: str):
        if device := device_registry.async_get(self.hass).async_get(device_id):
            _LOGGER.debug(f"_config_entry_by_device_id: device = {device}")
            for entry_id in device.config_entries:
                if entry := self.hass.config_entries.async_get_entry(entry_id):
                    _LOGGER.debug(f"_config_entry_by_device_id: entry: {entry}")
                    if entry.domain == "esphome":
                        return entry
        return None
    
    async def async_load(self):
        self.load_options()
        conf_entry = self._config_entry_by_device_id(self._config[CONF_DEVICE_ID])
        _LOGGER.debug(f"async_load: {self._config} {conf_entry}")

        if not conf_entry:
            return

        def _on_config_entry(state, entry):
            if entry == conf_entry:
                _LOGGER.debug(f"_on_config_entry: {state}, {entry}")
                if entry.state == ConfigEntryState.LOADED:
                    self._connect_to_esphome_device(entry.runtime_data)
                if entry.state == ConfigEntryState.NOT_LOADED:
                    self._disconnect_from_esphome()
        self._on_config_entry_handler = dispatcher.async_dispatcher_connect(self.hass, SIGNAL_CONFIG_ENTRY_CHANGED, _on_config_entry)
        if conf_entry.state == ConfigEntryState.LOADED:
            _on_config_entry(ConfigEntryChange.UPDATED, conf_entry)

    def _disable_listener(self, listener):
        if listener:
            listener()
        return None

    async def async_unload(self):
        _LOGGER.debug(f"async_unload:")
        self._on_config_entry_handler = self._disable_listener(self._on_config_entry_handler)
        self._on_entity_state_handler = self._disable_listener(self._on_entity_state_handler)
        self._disconnect_from_services()
        self._disconnect_from_esphome()
        self._dashboard = None

class BaseEntity(CoordinatorEntity):

    def __init__(self, coordinator):
        super().__init__(coordinator)

    def with_name(self, suffix: str, name: str):
        self._attr_has_entity_name = True
        entry_id = self.coordinator._entry_id
        self._attr_unique_id = f"_{entry_id}_{suffix}"
        self._attr_name = name
        return self

    @property
    def device_info(self):
        return {
            "identifiers": {
                ("config_entry", self.coordinator._entry_id)
            },
            "name": self.coordinator._config[CONF_NAME],
        }

    @property
    def available(self):
        return self.coordinator.is_device_connected()
