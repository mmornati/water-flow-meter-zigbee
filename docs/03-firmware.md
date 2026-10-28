# 03 · Firmware

## Build and flash

### PlatformIO (recommended)

```bash
cd firmware
pio run -t erase && pio run -t upload
pio device monitor
```

Erase only on the first flash (it also wipes the stored total and calibration). Later updates: `pio run -t upload`.

### Arduino IDE

1. Install the **esp32** core by Espressif, 3.3.0 or newer (Boards Manager).
2. Install **Adafruit BME280 Library** (Library Manager), even without a BME280: the sketch includes it.
3. Open `firmware/flow_meter/flow_meter.ino` and set the **Tools** menu:

| Setting | Value |
|---|---|
| Board | XIAO_ESP32C6 |
| Zigbee mode | Zigbee ZCZR (coordinator/router) |
| Partition Scheme | Zigbee ZCZR 4MB with spiffs |
| Erase All Flash Before Sketch Upload | Enabled (first flash only) |

Remember to [disconnect the TP4056 `OUT+`](01-hardware-and-wiring.md#connection-table) before plugging USB into the XIAO.

## How it works

| What | How |
|---|---|
| Counting | The ESP32-C6 **PCNT** hardware counter counts rising edges on `D2`, with a 10 µs glitch filter. No interrupt per pulse, nothing missed while the radio is busy |
| Flow | Sampled every second, averaged over the last 5 s, in L/min |
| Total | Kept in litres as a double; exact to the millilitre for decades |
| Reports | Water starting or stopping is sent right away; while flowing, every 5 s if flow moved ≥ 0.1 L/min or total ≥ 0.5 L; everything again every 5 min |
| Flash writes | Total saved once each time the water stops, and every 10 min during a long flow. Typically a few dozen writes a day |
| Network down | Water is still counted and saved; values are sent as soon as it rejoins |

### Zigbee endpoints

| EP | Cluster | Z2M property | Unit |
|---|---|---|---|
| 1 | Analog Input | `flow` | L/min |
| 2 | Analog Input | `water_consumed` | L |
| 2 | Analog Output (write) | set `water_consumed` | L |
| 3 | Analog Output | `pulses_per_liter` | pulses/L |
| 4 | Analog Input | `battery_voltage`, `battery` | V, % |
| 10 | Temperature + Humidity | `temperature`, `humidity` | °C, % |
| 11 | Pressure | `pressure` | hPa |

Endpoint 4 exists only with [battery sensing](#battery-sensing) enabled; 10 and 11 only if a BME280 answered at boot. After adding either one later, re-pair the device (or *Reconfigure* + *Interview* in Z2M) so Zigbee2MQTT learns the new endpoints.

## Calibration

Hall sensors are sold as "F = 5.5 × Q" or similar, but the real number of pulses per litre varies from unit to unit and with the flow. The firmware stores it in flash; change it from Home Assistant or Z2M (`pulses_per_liter`), no reflash needed.

With the Home Assistant package ([guide](04-zigbee2mqtt-and-home-assistant.md#calibration)):

1. Run **Water calibration - start**.
2. Fill a container of known volume (10 L bucket, watering can...) at your **usual flow**, from a tap after the sensor. Make sure nothing else uses water meanwhile.
3. Close the tap and wait until `flow` shows 0.
4. Set **Water calibration - real volume** and run **Water calibration - finish**. It computes `new = current × counted / real` and applies it.

By hand: `new pulses_per_liter = current × litres counted by the meter / litres really used`.

Changing the calibration never rewrites the litres already counted.

## Aligning the total with your water meter

Read your utility meter (say 254.318 m³ = 254318 L) and publish it once:

```bash
mosquitto_pub -t zigbee2mqtt/water_meter/set -m '{"water_consumed": 254318}'
```

Or from Zigbee2MQTT's *Dev console*: endpoint 2, cluster `genAnalogOutput`, write `presentValue`. Use 0 to reset.

## Battery sensing

1. Wire the divider from the 18650 `+` to `D0` ([wiring](01-hardware-and-wiring.md#connection-table)).
2. In `firmware/platformio.ini`, uncomment `-DBATTERY_ADC_PIN=0` (Arduino IDE: change `#define BATTERY_ADC_PIN -1` to `0` in the sketch).
3. Flash and re-pair. `battery_voltage` is reported every minute, `battery` is estimated linearly between 3.3 V (0 %) and 4.2 V (100 %).

## Settings

All in the `Behaviour` block at the top of `flow_meter.ino`:

| Define | Default | Meaning |
|---|---|---|
| `DEFAULT_PULSES_PER_LITER` | 490 | Used until a calibration is stored |
| `REPORT_MIN_INTERVAL_MS` | 5000 | Fastest report rate while water flows |
| `HEARTBEAT_MS` | 5 min | Re-send everything |
| `FLOW_REPORT_DELTA` / `TOTAL_REPORT_DELTA` | 0.1 L/min / 0.5 L | Change needed for a report |
| `SAVE_MAX_INTERVAL_MS` | 10 min | Flash write during a long flow |
| `CLIMATE_REPORT_MS` | 5 min | BME280 report rate |
| `PCNT_GLITCH_NS` | 10000 | Pulses shorter than this are noise |

## LED and button

| LED | Meaning |
|---|---|
| Slow blink | Not joined: open *Permit join* |
| Off | Joined |
| Fast blink | BOOT button held |

Hold **BOOT** for 3 s to leave the network (Zigbee factory reset). The total and the calibration are kept.

## Tests

The counting, reporting and saving logic lives in `flow_logic.h`, with no Arduino dependency, and is unit tested on your computer:

```bash
cd firmware
pio test -e native
```

Next: [Zigbee2MQTT & Home Assistant](04-zigbee2mqtt-and-home-assistant.md).
