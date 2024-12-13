import re


DEFAULT_ICON = "bookmark"

#^\s*([\w\-]+):\s(\w+),
#    "$1": "$2",
FIXED_DOMAIN_ICONS = {
    "air_quality": "mdiAirFilter",
    "alert": "mdiAlert",
    "calendar": "mdiCalendar",
    "climate": "mdiThermostat",
    "configurator": "mdiCog",
    "conversation": "mdiMicrophoneMessage",
    "counter": "mdiCounter",
    "datetime": "mdiCalendarClock",
    "date": "mdiCalendar",
    "demo": "mdiHomeAssistant",
    "google_assistant": "mdiGoogleAssistant",
    "group": "mdiGoogleCirclesCommunities",
    "homeassistant": "mdiHomeAssistant",
    "homekit": "mdiHomeAutomation",
    "image": "mdiImage",
    "image_processing": "mdiImageFilterFrames",
    "input_button": "mdiButtonPointer",
    "input_datetime": "mdiCalendarClock",
    "input_number": "mdiRayVertex",
    "input_select": "mdiFormatListBulleted",
    "input_text": "mdiFormTextbox",
    "lawn_mower": "mdiRobotMower",
    "light": "mdiLightbulb",
    "mailbox": "mdiMailbox",
    "notify": "mdiCommentAlert",
    "number": "mdiRayVertex",
    "persistent_notification": "mdiBell",
    "plant": "mdiFlower",
    "proximity": "mdiAppleSafari",
    "remote": "mdiRemote",
    "scene": "mdiPalette",
    "schedule": "mdiCalendarClock",
    "script": "mdiScriptText",
    "select": "mdiFormatListBulleted",
    "sensor": "mdiEye",
    "simple_alarm": "mdiBell",
    "siren": "mdiBullhorn",
    "stt": "mdiMicrophoneMessage",
    "text": "mdiFormTextbox",
    "time": "mdiClock",
    "timer": "mdiTimerOutline",
    "tts": "mdiSpeakerMessage",
    "updater": "mdiCloudUpload",
    "vacuum": "mdiRobotVacuum",
    "wake_word": "mdiChatSleep",
    "zone": "mdiMapMarkerRadius",
}

DEVICE_CLASS_ICONS = {
    "apparent_power": "mdiFlash",
    "aqi": "mdiAirFilter",
    "atmospheric_pressure": "mdiThermometerLines",
    "carbon_dioxide": "mdiMoleculeCo2",
    "carbon_monoxide": "mdiMoleculeCo",
    "current": "mdiCurrentAc",
    "data_rate": "mdiTransmissionTower",
    "data_size": "mdiDatabase",
    "date": "mdiCalendar",
    "distance": "mdiArrowLeftRight",
    "duration": "mdiProgressClock",
    "energy": "mdiLightningBolt",
    "frequency": "mdiSineWave",
    "gas": "mdiMeterGas",
    "humidity": "mdiWaterPercent",
    "illuminance": "mdiBrightness5",
    "irradiance": "mdiSunWireless",
    "moisture": "mdiWaterPercent",
    "monetary": "mdiCash",
    "nitrogen_dioxide": "mdiMolecule",
    "nitrogen_monoxide": "mdiMolecule",
    "nitrous_oxide": "mdiMolecule",
    "ozone": "mdiMolecule",
    "ph": "mdiPh",
    "pm1": "mdiMolecule",
    "pm10": "mdiMolecule",
    "pm25": "mdiMolecule",
    "power": "mdiFlash",
    "power_factor": "mdiAngleAcute",
    "precipitation": "mdiWeatherRainy",
    "precipitation_intensity": "mdiWeatherPouring",
    "pressure": "mdiGauge",
    "reactive_power": "mdiFlash",
    "signal_strength": "mdiWifi",
    "sound_pressure": "mdiEarHearing",
    "speed": "mdiSpeedometer",
    "sulphur_dioxide": "mdiMolecule",
    "temperature": "mdiThermometer",
    "timestamp": "mdiClock",
    "volatile_organic_compounds": "mdiMolecule",
    "volatile_organic_compounds_parts": "mdiMolecule",
    "voltage": "mdiSineWave",
    "volume": "mdiCarCoolantLevel",
    "water": "mdiWater",
    "weight": "mdiWeight",
    "wind_speed": "mdiWeatherWindy",
}

WEATHER_ICONS = {
    "clear-night": "mdiWeatherNight",
    "cloudy": "mdiWeatherCloudy",
    "exceptional": "mdiAlertCircleOutline",
    "fog": "mdiWeatherFog",
    "hail": "mdiWeatherHail",
    "lightning": "mdiWeatherLightning",
    "lightning-rainy": "mdiWeatherLightningRainy",
    "partlycloudy": "mdiWeatherPartlyCloudy",
    "pouring": "mdiWeatherPouring",
    "rainy": "mdiWeatherRainy",
    "snowy": "mdiWeatherSnowy",
    "snowy-rainy": "mdiWeatherSnowyRainy",
    "sunny": "mdiWeatherSunny",
    "windy": "mdiWeatherWindy",
    "windy-variant": "mdiWeatherWindyVariant",
}

#^\s*case "(\w+)":\n\s+return is_off \? (\w+) : (\w+);
#    "$1": ("$2", "$3"),
BINARY_SENSOR_ICONS = {
    "battery": ("mdiBattery", "mdiBatteryOutline"),
    "battery_charging": ("mdiBattery", "mdiBatteryCharging"),
    "carbon_monoxide": ("mdiSmokeDetector", "mdiSmokeDetectorAlert"),
    "cold": ("mdiThermometer", "mdiSnowflake"),
    "connectivity": ("mdiCloseNetworkOutline", "mdiCheckNetworkOutline"),
    "door": ("mdiDoorClosed", "mdiDoorOpen"),
    "garage_door": ("mdiGarage", "mdiGarageOpen"),
    "power": ("mdiPowerPlugOff", "mdiPowerPlug"),
    "gas": ("mdiCheckCircle", "mdiAlertCircle"),
    "problem": ("mdiCheckCircle", "mdiAlertCircle"),
    "safety": ("mdiCheckCircle", "mdiAlertCircle"),
    "tamper": ("mdiCheckCircle", "mdiAlertCircle"),
    "smoke": ("mdiSmokeDetectorVariant", "mdiSmokeDetectorVariantAlert"),
    "heat": ("mdiThermometer", "mdiFire"),
    "light": ("mdiBrightness5", "mdiBrightness7"),
    "lock": ("mdiLock", "mdiLockOpen"),
    "moisture": ("mdiWaterOff", "mdiWater"),
    "motion": ("mdiMotionSensorOff", "mdiMotionSensor"),
    "occupancy": ("mdiHomeOutline", "mdiHome"),
    "opening": ("mdiSquare", "mdiSquareOutline"),
    "plug": ("mdiPowerPlugOff", "mdiPowerPlug"),
    "presence": ("mdiHomeOutline", "mdiHome"),
    "running": ("mdiStop", "mdiPlay"),
    "sound": ("mdiMusicNoteOff", "mdiMusicNote"),
    "update": ("mdiPackage", "mdiPackageUp"),
    "vibration": ("mdiCropPortrait", "mdiVibrate"),
    "window": ("mdiWindowClosed", "mdiWindowOpen"),
}
BINARY_SENSOR_ICONS_DEFAULT = ("mdiRadioboxBlank", "mdiCheckboxMarkedCircle")

#^\s*case "(\w+)":\n\s*return (\w+);
#    "$1": "$2",
LOCK_ICONS = {
    "unlocked": "mdiLockOpen",
    "jammed": "mdiLockAlert",
    "locking": "mdiLockClock",
    "unlocking": "mdiLockClock",
}
SPEAKER_ICONS = {
    "playing": "mdiSpeakerPlay",
    "paused": "mdiSpeakerPause",
    "off": "mdiSpeakerOff",
}
TV_ICONS = {
    "playing": "mdiTelevisionPlay",
    "paused": "mdiTelevisionPause",
    "off": "mdiTelevisionOff",
}
MEDIA_PLAYER_ICONS = {
    "playing": "mdiCastConnected",
    "paused": "mdiCastConnected",
    "off": "mdiCastOff",
}
EVENT_ICONS = {
    "doorbell": "mdiDoorbell",
    "button": "mdiGestureTapButton",
    "motion": "mdiMotionSensor",
}
ALARM_PANEL_ICONS = {
    "armed_away": "mdiShieldLock",
    "armed_vacation": "mdiShieldAirplane",
    "armed_home": "mdiShieldHome",
    "armed_night": "mdiShieldMoon",
    "armed_custom_bypass": "mdiSecurity",
    "pending": "mdiShieldOutline",
    "triggered": "mdiBellRing",
    "disarmed": "mdiShieldOff",
}
AUTOMATION_ICONS = {
    "unavailable": "mdiRobotConfused",
    "off": "mdiRobotOff"
}
BUTTON_ICONS = {
    "identify": "mdiCrosshairsQuestion",
    "restart": "mdiRestart",
    "update": "mdiPackageUp",
}
COVER_ICONS = {
    "opening": "mdiArrowUpBox",
    "closing": "mdiArrowDownBox",
    "closed": "mdiWindowClosed",
}

def _icon_from_state(state) -> str:
    d = state.domain
    s = state.state
    dc = state.attributes.get("device_class")
    if d == "alarm_control_panel":
        return ALARM_PANEL_ICONS.get(s, "mdiShield")
    elif d == "automation":
        return AUTOMATION_ICONS.get(s, "mdiRobot")
    elif d == "binary_sensor":
        pair = BINARY_SENSOR_ICONS.get(dc, BINARY_SENSOR_ICONS_DEFAULT)
        return pair[0] if s == "off" else pair[1]
    elif d == "button":
        return BUTTON_ICONS.get(dc, "mdiButtonPointer")
    elif d == "camera":
        return "mdiVideoOff" if s == "off" else "mdiVideo"
    elif d == "cover":
        return COVER_ICONS.get(s, "mdiWindowOpen")
    elif d == "device_tracker":
        st = state.attributes.get("source_type")
        if st == "router":
            return "mdiLanConnect" if s == "home" else "mdiLanDisconnect"
        elif st in ("bluetooth", "bluetooth_le"):
            return "mdiBluetoothConnect" if s == "home" else "mdiBluetooth"
        else:
            return "mdiAccountArrowRight" if s == "not_home" else "mdiAccount"
    elif d == "event":
        return EVENT_ICONS.get(dc, "mdiEyeCheck")
    elif d == "fan":
        return "mdiFanOff" if s == "off" else "mdiFan"
    elif d == "humidifier":
        return "mdiAirHumidifierOff" if s == "off" else "mdiAirHumidifier"
    elif d == "input_boolean":
        return "mdiCheckCircleOutline" if s == "on" else "mdiCloseCircleOutline"
    elif d == "input_datetime":
        if "has_date" not in state.attributes:
            return "mdiClock"
        if "has_time" not in state.attributes:
            return "mdiCalendar"
    elif d == "lock":
        return LOCK_ICONS.get(s, "mdiLock")
    elif d == "media_player":
        if dc == "speaker":
            return SPEAKER_ICONS.get(s, "mdiSpeaker")
        elif dc == "tv":
            return TV_ICONS.get(s, "mdiTelevision")
        elif dc == "receiver":
            return "mdiAudioVideoOff" if s == "off" else "mdiAudioVideo"
        else:
            return MEDIA_PLAYER_ICONS.get(s, "mdiCast")
    elif d in ("number", "sensor"):
        if dc == "battery":
            return "mdiBatteryCharging" if s == "on" else "mdiBattery"
        elif dc in DEVICE_CLASS_ICONS:
            return DEVICE_CLASS_ICONS.get(dc)
    elif d == "person":
        return "mdiAccountArrowRight" if s == "not_home" else "mdiAccount"
    elif d == "switch":
        if dc == "outlet":
            return "mdiPowerPlug" if s == "on" else "mdiPowerPlugOff"
        elif dc == "switch":
            return "mdiToggleSwitchVariant" if s == "on" else "mdiToggleSwitchVariantOff"
        else:
            return "mdiToggleSwitchVariant"
    elif d == "sun":
        return "mdiWhiteBalanceSunny" if s == "above_horizon" else "mdiWeatherNight"
    elif d == "switch_as_x":
        return "mdiSwapHorizontal"
    elif d == "threshold":
        return "mdiChartSankey"
    elif d == "update":
        return "mdiPackageUp" if s == "on" else "mdiPackage"
    elif d == "water_heater":
        return "mdiWaterBoilerOff" if s == "off" else "mdiWaterBoiler"
    elif d == "weather":
        return WEATHER_ICONS.get(s, "mdiWeatherCloudy")
    elif d in FIXED_DOMAIN_ICONS:
        return FIXED_DOMAIN_ICONS[d]
    return DEFAULT_ICON

def _normalize_icon(icon: str) -> str:
    if icon and icon[:4] == "mdi:":
        return icon[4:]
    if icon and icon[:3] == "mdi":
        return re.sub(r"[A-Z0-9]", lambda x: f"-{x[0].lower()}", icon)[4:]
    return icon

def icon_from_state(icon: str | None, state = None) -> str:
    if state and not icon:
        icon = state.attributes.get("icon")
    if state and not icon:
        icon = _icon_from_state(state)
    return _normalize_icon(icon)
