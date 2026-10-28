# 05 · Troubleshooting

🌐 **English** · [Français](fr/05-troubleshooting.md) · [Italiano](it/05-troubleshooting.md)

Start with the serial monitor (`pio device monitor`, [TP4056 `OUT+` disconnected](01-hardware-and-wiring.md#connection-table)): the firmware logs the calibration, every report and every flash write.

## The LED keeps blinking slowly

The meter is not joined.

- Open *Permit join* in Zigbee2MQTT, close to the meter.
- It was paired to another network before: hold **BOOT** 3 s to leave it, then pair again.
- Range: make sure the U.FL antenna is plugged in; a utility room full of pipes is a hard place for 2.4 GHz.

## No pulses at all

`Flow 0.00 L/min` while water runs.

- Measure the yellow wire with water flowing: it should toggle (a multimeter shows roughly half the supply voltage).
- Diode in the wrong direction: the bar must face the sensor.
- No pull-up on `D2`: add the 10 kΩ to `3V3`.
- The sensor doesn't run on 3.3–4.2 V: some only work from ~4.5 V. Check it with a 5 V supply. If that's the cause, add a small 5 V boost module (MT3608 or similar) between TP4056 `OUT` and the sensor's red wire only.
- Arrow on the sensor body against the water direction.

## Flow looks too high or too low

- Calibrate ([how](03-firmware.md#calibration)). 490 pulses/L is only a starting point.
- Very low flows (< 1 L/min, a dripping tap) are below what a 1–30 L/min turbine measures accurately: it may not spin at all.
- Erratic readings with no water: electrical noise on a long cable. Use twisted pair, keep it away from mains, add 100 nF from `D2` to `GND`, or raise `PCNT_GLITCH_NS`.

## `water_consumed` went back after a reboot

The total is saved when the water stops and every 10 min while it flows, so a hard reset loses at most the litres of the current use. A full flash erase (`pio run -t erase`, or *Erase All Flash* in the Arduino IDE) wipes it: [set it again](03-firmware.md#aligning-the-total-with-your-water-meter).

## Calibration from Home Assistant does nothing

- The calibration script refuses to apply while water flows: wait for `flow` = 0.
- `number.water_meter_pulses_per_liter` missing: the converter is not the current version, or the device needs *Reconfigure* in Z2M.

## Battery, temperature or pressure stay unknown

These endpoints only exist when the hardware is there at boot. After adding the BME280 or the battery divider (and `BATTERY_ADC_PIN`), reboot the meter, then *Interview* and *Reconfigure* it in Zigbee2MQTT.

The serial log at boot says `BME280 found` or `No BME280, climate endpoints disabled`. If it isn't found: SDA on `D7`, SCL on `D8`, powered from `3V3`, and it is a **BME**280 (a BMP280 has no humidity and a different chip ID).

## The meter dies quickly on battery

Expected: the Hall sensor (5–15 mA) and a Zigbee router that never sleeps empty an 18650 in one to two days. The battery is there to ride through power cuts. If the TP4056 never shows *full*, the charger is too weak or the cell is worn.
