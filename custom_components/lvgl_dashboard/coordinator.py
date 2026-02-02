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

from homeassistant.core import callback
from homeassistant.components import camera, image, light
from homeassistant.const import (
    CONF_DEVICE_ID,
    CONF_NAME,
)


from .constants import (
    DOMAIN,
    CONF_DASHBOARD,
    CONF_IS_BROWSER,
    TOGGLABLE_DOMAINS,
    SLIDER_DOMAINS,
    IMAGE_DOMAINS,
)

from .mdi_font.icon import icon_from_state

from .mdi_font import GlyphProvider
from .picture import bytes_to_565_ints, async_get_image_by_entity_id, bytes_to_scaled

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
        self._entry_data = None
        self._on_entity_state_handler = None
        self._on_event_handler = None

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
            theme_conf = self._g(self._dashboard, "theme", {})
            page_no = 0
            for page in self.all_items(self._dashboard, "pages"):
                item_no = 0
                cols = self._g(page, "cols", self._g(theme_conf, "cols", 4))
                for (_, _, _, _, item) in self._sections_page(self.all_items(page, "sections"), cols):
                    yield (page_no, page, item_no, item)
                    item_no += 1

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
                return template.render_complex(value, {"this": state, "state": state})
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
    
    def browser_image_url(self, entity_id: str, scale: int) -> str:
        return f"/api/lvgl_dashboard/image/{self._entry_id}/{entity_id}/{scale}?ts={datetime.now().timestamp()}"
    
    async def async_picture_by_entity_id(self, entity_id: str):
        domain, _ = entity_id.split(".")
        if domain == "camera":
            image_ = await camera.async_get_image(self.hass, entity_id, width=800, height=800)
            _LOGGER.debug(f"async_prepare_data: picture: {image_.content_type}, {len(image_.content)}")
            return image_
        if domain == "image":
            image_ = await async_get_image_by_entity_id(self.hass, entity_id)
            return image_
        return None
    
    async def async_picture_by_entity_id_scaled(self, entity_id: str, scale: int):
        try:
            if image_ := await self.async_picture_by_entity_id(entity_id):
                data = bytes_to_scaled(image_.content, scale)
                return (data, image_.content_type)
        except:
            _LOGGER.exception(f"async_picture_from_state: error getting picture")
        return (None, None)
    
    async def async_picture_from_state(self, entity_id: str | None, state, size: int, le: bool = False):
        try:
            if entity_id:
                if image_ := await self.async_picture_by_entity_id(entity_id):
                    size, data = bytes_to_565_ints(image_.content, image_.content_type, size, le)
                    return (size, data)
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

    async def async_send_picture_data(self, service: str, entity_id: str, scale: int, le: bool, cb):
        state = self.state_by_entity_id(entity_id)
        size, data = await self.async_picture_from_state(entity_id, state, scale, le)
        if size and data:
            offset = 0
            while offset < len(data):
                await self.async_call_device_service(service, {
                    "data": data[offset:(offset + SET_DATA_BATCH)], 
                    "offset": offset, "size": len(data),
                    **cb(),
                })
                offset += SET_DATA_BATCH

    def _add_with_priority(self, items: list, item: dict, x_field="col", y_field="row") -> list:
        for i in range(len(items)):
            item_ = items[i]
            if item_[x_field] == item[x_field] and item_[y_field] == item[y_field]:
                # Found
                same_col = (item["col"] != "" and item_["col"] != "") or (item["col"] == "" and item_["col"] == "")
                _LOGGER.debug(f"_add_with_priority: Found same cell: {item_} and {item}, same_col: {same_col}")
                if same_col:
                    if item_["p"] > item["p"]:
                        # Previous item has higher prio - skip item
                        return items
                else:
                    if item["col"] == "" and item_["col"] != "":
                        # Last item or item with color wins
                        return items
                items[i] = item
                return list(items)
        items.append(item)
        return list(items)

    def _to_font(self, item: dict, state) -> str:
        return {
            "n": "", "normal": "", 
            "small": "s", 
            "large": "l", "big": "l",
        }.get(self._g(item, "font", "", state=state))
    
    def get_theme_scale(self) -> float:
        theme_conf = self._g(self._dashboard, "theme", {})
        return self._g(theme_conf, "scale", 1.0)
    
    def get_more_page_image_scale(self):
        theme_conf = self._g(self._dashboard, "theme", {})
        return theme_conf.get("more_page_image_size") if "more_page_image_size" in theme_conf else int(PICTURE_DEF_SCALE_MORE * self.get_theme_scale())

    async def async_prepare_data(self, layout: str, item: dict) -> list:
        entity_id = self._g(item, "entity_id")
        [domain, name] = entity_id.split(".") if entity_id else ("", "")
        state = self.state_by_entity_id(entity_id)
        hidden = self._g(item, "hidden", state=state)
        theme_scale = self.get_theme_scale()
        if hidden:
            return {"_h": True}
        if layout == "button":
            icon = icon_from_state(self._g(item, "icon", state=state), state)
            icon_size = int(self._g(item, "size", ICON_LARGE, state=state) * theme_scale)
            return {
                "name": self._g(item, "name", self.name_from_state(entity_id, state), state=state),
                "icon": self._mdi_font.get_icon_value(icon, icon_size),
                "ctype": self._g(item, "ctype", "button"),
                "col": self.color_from_state(state, item),
                "font": self._to_font(item, state),
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
                "font": self._to_font(item, state),
            }
        if layout == "tile":
            icon = icon_from_state(self._g(item, "icon", state=state), state)
            icon_size = int(self._g(item, "size", ICON_SMALL, state=state))
            response = {
                "name": self._g(item, "name", self.name_from_state(entity_id, state), state=state),
                "icon": self._mdi_font.get_icon_value(icon, icon_size),
                "value": str(self._g(item, "value", state.state if state else "", state=state)),
                "col": self.color_from_state(state, item),
                "v": self._g(item, "vertical", False),
                "f": "b" if self._g(item, "features_position", "bottom") == "bottom" else "i",
                "t": domain in TOGGLABLE_DOMAINS,
                "ctype": "text",
            }
            if badge := self._g(item, "badge"):
                response["badge"] = badge
            return response
        if layout == "heading":
            icon = icon_from_state(self._g(item, "icon", state=state), state)
            icon_size = int(self._g(item, "size", ICON_SMALL, state=state))
            return {
                "name": self._g(item, "heading", self.name_from_state(entity_id, state), state=state),
                "icon": self._mdi_font.get_icon_value(icon, icon_size),
            }
        if layout == "picture":
            scale = self._g(item, "scale", PICTURE_DEF_SCALE_ITEM, state=state)
            size, data = await self.async_picture_from_state(entity_id, state, int(scale * theme_scale))
            if size and data:
                return {
                    "ctype": self._g(item, "ctype", "button"),
                    "col": self.color_from_state(state, item),
                    "image": {
                        "width": size[0], 
                        "height": size[1],
                        "uri": self.browser_image_url(entity_id, scale) if self.is_browser else None,
                    }
                }
        if layout == "layout":
            items = []
            ctype = self._g(item, "ctype", "button")
            result = {
                "ctype": ctype,
                "col": self.color_from_state(state, item),
                "cols": self._g(item, "lc", [1], state=state),
                "rows": self._g(item, "lr", [1], state=state),
            }
            cols = len(result["cols"])
            rows = len(result["rows"])
            col = 0
            row = 0
            for item_ in self.all_items(item, "items"):
                col, row, x, y, cs, rs = self._arrange_in_cells(item_, col, row, cols, rows)
                entity_id_ = self._g(item_, "entity_id", state=state)
                state_ = self.state_by_entity_id(entity_id_)

                if self._g(item_, "hidden", state=state_) == True:
                    continue
                shape = self._g(item_, "shape", "", state=state_)
                item__ = {
                    "ctype": self._g(item_, "ctype", "button" if shape else "text", state=state_),
                    "x": x,
                    "y": y,
                    "w": cs,
                    "h": rs,
                    "col": self.color_from_state(state_, item_),
                    "p": self._g(item_, "prio", 0, state=state_),
                    "shp": shape,
                    "r": int(self._g(item_, "radius", 0, state=state_) * theme_scale),
                    "font": self._to_font(item_, state_),
                }
                if "label" in item_:
                    item__["label"] = self._g(item_, "label", state=state_)
                else:
                    icon_size = self._g(item_, "size", ICON_SMALL, state=state_)
                    item__["icon"] = self._mdi_font.get_icon_value(
                        icon_from_state(self._g(item_, "icon", state=state_), state_), 
                        int(icon_size * theme_scale)
                    )
                items = self._add_with_priority(items, item__, x_field="x", y_field="y")
            result["items"] = items
            return result
        return None

    async def async_send_values(self, entity_id: str | None = None, page: int | None = None):
        for (page_no, _, item_no, item) in self._dashboard_items():
            if (entity_id is None or entity_id in self._pick_entity_ids(item)) and (page is None or page == page_no):
                type_ = self._g(item, "type", self._g(item, "layout", "button"))
                if op := await self.async_prepare_data(type_, item):
                    _LOGGER.debug(f"async_send_values: set_value: {page_no}, {item_no}, {op}")
                    await self.async_call_device_service("set_value", {
                        "page": page_no, "item": item_no, "json_value": json.dumps(op)
                    })

    async def async_prepare_button(self, item: dict):
        result = {
            "clk": self._g(item, "click_sound", def_=False),
            "lclk": self._g(item, "long_click_sound", def_=False),
        }
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

    def _arrange_in_cells(self, item, col, row, cols, rows):
        x = self._g(item, "col", col)
        y = self._g(item, "row", row)
        if x < 0:
            x = cols + x
        if y < 0:
            y = rows + y
        cs = self._g(item, "cols", 1)
        rs = self._g(item, "rows", 1)
        col = x + cs
        row = y
        if col >= cols:
            col = 0
            row += 1 
        return (col, row, x, y, cs, rs)
    
    def _item_size(self, item: dict, cols: int):
        type_ = self._g(item, "type", self._g(item, "layout", "button"))
        w = 2
        h = 2
        if type_ == "heading":
            return (cols, 1)
        elif type_ == "tile":
            feat_bottom = self._g(item, "features_position", "bottom") == "bottom",
            w = 2
            h = 1
            if self._g(item, "vertical", False):
                w = 1
                h = 2
            else:
                if not feat_bottom:
                    w = 4
        return (self._g(item, "cols", w), self._g(item, "rows", h))
    
    def _sections_layout(self, cols: int, items: list, compact: bool = False):
        data = []
        result = []
        last_y = 0
        for item in items:
            w = self._g(item, "cols", 4) # Default section size
            h = self._g(item, "rows", 1)
            item_data = None
            if "cards" in item:
                [item_data, h] = self._sections_layout(w, self._g(item, "cards", []), True)
            else:
                [w, h] = self._item_size(item, cols)
                item_data = item
            sx = 0
            sy = len(data)
            found = False
            for y in range(last_y, len(data)):
                for x in range(cols):
                    if data[y][x] == None:
                        gap = 0
                        for i in range(x, cols):
                            if data[y][i] != None:
                                break
                            gap += 1
                        # That's how much available
                        if gap >= w:
                            sx = x
                            sy = y
                            found = True
                            break
                if found:
                    break
            while len(data) < sy + h:
                data.append([None for _ in range(cols)])
            for x in range(w):
                for y in range(h):
                    data[sy + y][sx + x] = True
            # _LOGGER.debug(f"_sections_layout: {sx}x{sy}x{w}x{h}: {data}")
            last_y = sy
            result.append((sx, sy, w, h, item_data))
        return (result, len(data))

    def _sections_page(self, items: list, cols: int):
        (sections, _) = self._sections_layout(cols, items, False)
        for (x, y, w, h, item_data) in sections:
            if isinstance(item_data, list):
                for sitem in item_data:
                    [sx, sy, w, h, sitem_data] = sitem
                    yield (x + sx, y + sy, w, h, sitem_data)
            else:
                    yield (x, y, w, h, item_data)

    async def async_send_dashboard(self):
        self._on_entity_state_handler = self._disable_listener(self._on_entity_state_handler)
        name = self._config.get(CONF_DASHBOARD)
        if not name:
            name = "default"
        dashboard = self.get_dashboard(name)
        self._dashboard = self._to_template(dashboard)
        theme = {}
        theme_conf = self._g(self._dashboard, "theme", {})
        for key in theme_conf:
            value = self._g(self._dashboard["theme"], key)
            if value or value == 0:
                theme[key] = str(value)
        _LOGGER.debug(f"async_send_dashboard: set_theme: {theme}")
        await self.async_call_device_service("set_theme", {"json_value": json.dumps(theme)})
        all_entity_ids = set()
        page_index = 0
        for page in self.all_items(self._dashboard, "pages"):
            rows = self._g(page, "rows", self._g(theme_conf, "rows", 4))
            cols = self._g(page, "cols", self._g(theme_conf, "cols", 4))
            page_data = {
                "rows": rows, 
                "cols": cols, 
                "items": []
            }
            for (x, y, w, h, item) in self._sections_page(self.all_items(page, "sections"), cols):
                type_ = self._g(item, "type", self._g(item, "layout", "button"))
                item_data = {
                    "layout": type_,
                    "col": x,
                    "row": y,
                    "cols": w,
                    "rows": h,
                }
                all_entity_ids.update(self._pick_entity_ids(item))
                page_data["items"].append(item_data)
            _LOGGER.debug(f"async_send_dashboard: sections: {page_data}")

            col = 0
            row = 0
            for item in self.all_items(page, "items"):
                col, row, x, y, cs, rs = self._arrange_in_cells(item, col, row, cols, rows)
                _LOGGER.debug(f"async_send_dashboard: {x}x{y} - {cs}x{rs}, {col}x{row}, {item}, {cols}")
                item_data = {
                    "layout": self._g(item, "layout", "button"),
                    "col": x,
                    "row": y,
                    "cols": cs,
                    "rows": rs,
                }
                all_entity_ids.update(self._pick_entity_ids(item))
                page_data["items"].append(item_data)
            
            await self.async_call_device_service("add_page", {"json_value": json.dumps(page_data), "reset": page_index == 0})
            page_index += 1
        idx = 0
        for button in self._dashboard.get("buttons", []):
            await self.async_call_device_service("set_button", {
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

    async def async_send_show_more_page(self, entity_id: str, item: dict | None = None, immediate: bool = False):
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
            size, data = await self.async_picture_from_state(entity_id, state, self.get_more_page_image_scale())
            if size and data:
                features.append({
                    "type": "image", 
                    "width": size[0], 
                    "height": size[1],
                    "id": "image",
                })
        await self.async_call_device_service("show_more", {
            "json_value": json.dumps({
                "features": features, "id": entity_id,
                "immediate": immediate,
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

    async def async_call_device_service(self, name: str, data: dict) -> bool:
        if self.is_browser:
            _LOGGER.debug(f"call_device_service: browser entry, emit event {name} with {data}")
            self.hass.bus.async_fire("lvgl_dashboard_service_call", {
                "config_entry_id": self._entry_id,
                "name": name,
                "data": data,
            })
            return
        if not self.is_device_connected():
            _LOGGER.debug(f"call_device_service: not connected")
            return False
        for _, service in self._entry_data.services.items():
            if service.name == name:
                # _LOGGER.debug(f"call_device_service: call service {name} with {data}, spec: {service}")
                # self.hass.async_add_executor_job(self._entry_data.client.execute_service, service, data)
                await self._entry_data.client.execute_service(service, data)
                return True
        _LOGGER.warning(f"call_device_service: service not found: {name}")
        return False

    def is_device_connected(self) -> bool:
        if self.is_browser:
            return True
        return True if self._entry_data and self._entry_data.available else False
    
    def _get_item_def(self, page: int, item: int) -> dict | None:
        for (page_no, _, item_no, item_data) in self._dashboard_items():
            if page == page_no and item == item_no:
                return item_data
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
            action = id
        elif domain == "automation":
            action = "automation.trigger"
        elif domain == "scene":
            action = "scene.apply"
        elif domain == "button":
            action = "button.press"
        if "action" in action_def:
            action = self._g(action_def, "action")
        extra = self._g(action_def, "extra", {})
        
        _LOGGER.debug(f"async_execute_hass_action: action = {action}, entity = {id}, extra={extra}")
        [domain, name] = action.split(".")
        await self.hass.services.async_call(domain, name, {
            "entity_id": id,
            **extra,
        }, blocking=False)

    async def async_send_show_page(self, index: int):
        await self.async_call_device_service("show_page", {"page": index})

    async def async_send_hide_more_page(self):
        await self.async_call_device_service("hide_more", {})
    
    async def async_send_play_rtttl(self, song: str):
        await self.async_call_device_service("play_rtttl", {"song": song})

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
            await self.async_send_show_more_page(entity_id, item_def, immediate=True)

    async def async_handle_event(self, type_: str, event: dict):
        _LOGGER.debug(f"async_handle_event: event = {event}")
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
            if not visible and changed and not self.is_browser:
                await self.async_send_values(page=self.data.get("page", 0))
        if type_ in ("button", "long_button"):
            btns = self._dashboard.get("buttons", [])
            if 0 <= item < len(btns):
                btn = btns[item]
                await self.async_exec_action(self._g(btn, "on_tap" if type_ == "button" else "on_long_tap"), btn)
        le = event.get("le") == "1"
        if item_def := self._get_item_def(page, item):
            item_type_ = self._g(item_def, "type", self._g(item_def, "layout", "button"))
            if type_ == "click":
                action = self._g(item_def, "on_tap")
                if not action and item_type_ == "tile":
                    action = { "toggle": True }
                await self.async_exec_action(action, item_def)
            if type_ == "long_press":
                action = self._g(item_def, "on_long_tap")
                if not action and item_type_ == "tile":
                    action = { "more": True }
                await self.async_exec_action(action, item_def)
            if type_ == "data_request":
                entity_id_ = self._g(item_def, "entity_id")
                scale = int(self._g(item_def, "scale", PICTURE_DEF_SCALE_ITEM) * self.get_theme_scale())
                await self.async_send_picture_data("set_data", entity_id_, scale, le, lambda: {"item": item, "page": page})
        if entity_id and op:
            if type_ == "change":
                value = int(event.get("value", 0))
                await self.async_exec_change_action(entity_id, op, value)
            if type_ == "data_request":
                await self.async_send_picture_data("set_data_more", entity_id, self.get_more_page_image_scale(), le, lambda: {})

    def _connect_to_esphome_device(self, entry_data):
        def _on_device_update():
            _LOGGER.debug(f"_on_device_update: {entry_data.available}")
            if entry_data.available:
                self.hass.async_create_task(self.async_send_dashboard())
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

    @property
    def is_browser(self):
        return self._config.get(CONF_IS_BROWSER, False) or self._config.get(CONF_DEVICE_ID) is None

    def _config_entry_by_device_id(self, device_id: str):
        if device := device_registry.async_get(self.hass).async_get(device_id):
            _LOGGER.debug(f"_config_entry_by_device_id: device = {device}")
            for entry_id in device.config_entries:
                if entry := self.hass.config_entries.async_get_entry(entry_id):
                    _LOGGER.debug(f"_config_entry_by_device_id: entry: {entry}")
                    if entry.domain == "esphome":
                        return entry
        return None
    
    async def async_handle_web_event(self, event: str, data: dict):
        _LOGGER.debug(f"async_handle_web_event: {event}, {data}")
        if event == "online":
            return await self.async_send_dashboard()
        if event in ("click", "long_click", "page", "more"):
            return await self.async_handle_event(event, data)

    async def _async_on_device_event(self, event):
        type_ = event.data.get("type")
        await self.async_handle_event(type_, event.data)

    @callback
    def _device_event_filter(self, event_data) -> bool:
        return self.is_device_connected() and event_data.get("device_id") == self._config[CONF_DEVICE_ID]

    async def async_load(self):
        self.load_options()
        if self.is_browser:
            _LOGGER.debug(f"async_load: browser entry: {self._config}")
            self._on_config_entry_handler = None
            return
        conf_entry = self._config_entry_by_device_id(self._config[CONF_DEVICE_ID])
        _LOGGER.debug(f"async_load: {self._config} {conf_entry}")
        if not conf_entry:
            return
        
        self._on_event_handler = self.hass.bus.async_listen("esphome.lvgl_dashboard_event", self._async_on_device_event, self._device_event_filter)

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
        self._on_event_handler = self._disable_listener(self._on_event_handler)
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
