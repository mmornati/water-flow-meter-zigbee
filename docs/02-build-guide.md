# 02 · Step-by-step build

## 1. Bench test first

Before anything goes near a pipe:

1. Flash the firmware with only the XIAO on USB ([Firmware](03-firmware.md)).
2. Open the serial monitor (`pio device monitor`). You should see the calibration, the stored total and `Waiting for the network; water is counted meanwhile`.
3. Wire the sensor signal as in [Hardware & wiring](01-hardware-and-wiring.md#the-pulse-signal-read-this-before-wiring), power the sensor from the XIAO's `5V` pin (5 V from USB, so the diode is a must here), and **blow gently into the sensor**: the serial log prints a non-zero `Flow ... L/min` within a few seconds.
4. Pair it with Zigbee2MQTT ([Zigbee2MQTT & Home Assistant](04-zigbee2mqtt-and-home-assistant.md)) while it is still on the bench.

## 2. Power

1. Wire the TP4056 `OUT+`/`OUT-` to the XIAO `5V`/`GND` through a plug you can disconnect (for flashing later).
2. Insert the 18650, then plug the USB charger into the TP4056. The module's red LED means charging, blue/green means full.
3. Optional: wire the battery divider to `D0` and enable it in the firmware ([battery sensing](03-firmware.md#battery-sensing)).

## 3. Plumbing

- The arrow on the sensor body shows the water direction. Mounted backwards it still spins, but counts badly.
- Install it **after the main meter and the pressure reducer**, on the pipe that feeds the whole house, or on a single branch (garden, water heater) if that is what you want to follow.
- Use the gaskets and PTFE tape on the G3/4 threads. Most sensors are rated ~1.75 MPa (17.5 bar): fine behind a pressure reducer, not on a raw high-pressure supply.
- Keep the electronics **above** the sensor, away from drips, in an IP65 box. Bring the cable in from below with a drip loop.
- A Hall sensor is a small turbine: put a filter upstream if your water carries sand, or the rotor will wear and under-count.

## 4. Enclosure

- ABS/polycarbonate boxes don't block Zigbee. Metal ones do: put the U.FL antenna outside.
- Fix the 18650 holder firmly; leave the TP4056 USB-C port reachable or extend it through a gland.
- Optional BME280: mount it on the **outside** of the box (or in a vented corner), otherwise it measures the electronics, not the room.

## 5. Calibrate

The default is 490 pulses per litre, the value from the ESPHome version. Every sensor is a bit different and the number depends on the flow; take ten minutes to check it with a bucket ([calibration](03-firmware.md#calibration)).

## 6. Final checks

| Check | Expected |
|---|---|
| Open a tap | `flow` in Z2M goes above 0 within 5 s |
| Close it | `flow` back to 0 within ~5 s; `water_consumed` increased |
| Unplug the USB charger | The meter keeps reporting (on battery) |
| Reboot it (unplug everything) | `water_consumed` comes back with the same value |
| Zigbee2MQTT map | The meter shows as a **router** |

Next: [Firmware](03-firmware.md).
