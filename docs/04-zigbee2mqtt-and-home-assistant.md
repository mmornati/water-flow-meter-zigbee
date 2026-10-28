# 04 · Zigbee2MQTT & Home Assistant

🌐 **English** · [Français](fr/04-zigbee2mqtt-and-home-assistant.md) · [Italiano](it/04-zigbee2mqtt-and-home-assistant.md)

## Install the converter

1. Copy [`zigbee2mqtt/water_flow_meter.mjs`](../zigbee2mqtt/water_flow_meter.mjs) to `<z2m data dir>/external_converters/water_flow_meter.mjs`.
   With the Home Assistant add-on that is `/config/zigbee2mqtt/external_converters/` (or `/homeassistant/zigbee2mqtt/...` depending on the add-on version).
2. Restart Zigbee2MQTT. The log shows `Loaded external converter 'water_flow_meter.mjs'`.

## Pair

1. Click **Permit join** in Zigbee2MQTT.
2. Power the meter. Its LED blinks slowly while searching and turns off once joined.
3. Rename it **`water_meter`** (the Home Assistant package assumes this name), with *Update Home Assistant entity ID* ticked.

If the device shows as *Unsupported*, the converter wasn't loaded: check the log, then *Reconfigure* the device.

## What you get

| Z2M property | Home Assistant entity | Notes |
|---|---|---|
| `flow` | `sensor.water_meter_flow` | L/min, device class *volume flow rate* |
| `water_consumed` | `sensor.water_meter_water_consumed` | L, device class *water*, *total increasing* |
| `pulses_per_liter` | `number.water_meter_pulses_per_liter` | Calibration, under *Configuration* |
| `battery_voltage`, `battery` | `sensor.water_meter_battery_voltage`, `sensor.water_meter_battery` | Only with battery sensing |
| `temperature`, `humidity`, `pressure` | `sensor.water_meter_temperature`, ... | Only with a BME280 |

Entities for optional sensors that aren't fitted simply stay *unknown*; hide them if you like.

## Energy dashboard

*Settings → Dashboards → Energy → Water consumption → Add water source* and pick `sensor.water_meter_water_consumed` (or the package's `sensor.water_total_m3`). Home Assistant handles litres and m³.

## The Home Assistant package

[`homeassistant/water_meter.yaml`](../homeassistant/water_meter.yaml) adds:

| Item | What it does |
|---|---|
| `sensor.water_today`, `_this_month`, `_this_year` | Utility meters on the total |
| `sensor.water_total_m3` | The total in m³, like your utility bill |
| **Continuous flow** alert | Water running without a break for N minutes (default 60, `input_number.water_leak_alert_minutes`): a leak, a hose or a tap left open |
| **Night usage** alert | More than 2 L used between 01:00 and 06:00: a dripping tap or a running toilet |
| **Meter offline** alert | No data for 15 min |
| **On battery** alert | Battery below 4.0 V for 5 min, which means USB power is gone (only with battery sensing) |
| **Calibration** scripts | See below |

Install:

1. Copy it to `/config/packages/water_meter.yaml`.
2. Make sure `configuration.yaml` contains:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

3. Replace `notify.notify` with your own notifier (for example `notify.mobile_app_<phone>`).
4. *Developer tools → Check configuration*, then restart.

### Calibration

1. Run **Water calibration - start** (it stores the current total).
2. Fill a container of known volume at your usual flow; nothing else may use water meanwhile.
3. Close the tap, wait until `sensor.water_meter_flow` is 0.
4. Set **Water calibration - real volume** to the litres you really poured and run **Water calibration - finish**.

The script writes the corrected `pulses_per_liter` to the meter and notifies you of the old and new values. Repeat once to confirm: the second run should change it by only a few pulses.

Next: [Troubleshooting](05-troubleshooting.md).
