from homeassistant.components import button
from homeassistant.helpers.entity import EntityCategory

from .coordinator import BaseEntity
from .constants import DOMAIN


async def async_setup_entry(hass, entry, async_setup_entities):
    async_setup_entities([_RefreshButton(entry.runtime_data)])

class _RefreshButton(BaseEntity, button.ButtonEntity):

    def __init__(self, coordinator):
        super().__init__(coordinator)
        self.with_name("refresh", "Refresh")
        self._attr_icon = "mdi:refresh"
        self._attr_entity_category = EntityCategory.CONFIG

    async def async_press(self) -> None:
        await self.coordinator.async_send_dashboard()
