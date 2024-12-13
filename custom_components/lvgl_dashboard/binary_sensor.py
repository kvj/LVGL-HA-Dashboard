from homeassistant.components.binary_sensor import BinarySensorEntity

import logging

from .coordinator import BaseEntity, Coordinator

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass, entry, add_entities):
    coordinator = entry.runtime_data
    add_entities([_MorePage(coordinator)])
    return True


class _MorePage(BaseEntity, BinarySensorEntity):

    def __init__(self, coordinator: Coordinator):
        super().__init__(coordinator)
        self.with_name("more_page", "More Page")
        self._attr_icon = "mdi:dots-horizontal"

    @property
    def is_on(self):
        return self.coordinator.data.get("more_page", False)
