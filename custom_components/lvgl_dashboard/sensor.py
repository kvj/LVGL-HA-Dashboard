from homeassistant.helpers.update_coordinator import (
    CoordinatorEntity,
)

from homeassistant.components.sensor import SensorEntity

import logging

from .constants import DOMAIN
from .coordinator import BaseEntity, Coordinator

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass, entry, add_entities):
    coordinator = entry.runtime_data
    add_entities([_Page(coordinator)])
    return True


class _Page(BaseEntity, SensorEntity):

    def __init__(self, coordinator: Coordinator):
        super().__init__(coordinator)
        self.with_name("page", "Page")
        self._attr_icon = "mdi:page-layout-body"
        self._attr_state_class = "measurement"
        self._attr_suggested_display_precision = 0

    @property
    def native_value(self):
        return self.coordinator.data.get("page")
