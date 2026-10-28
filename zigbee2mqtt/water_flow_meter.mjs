// Zigbee2MQTT external converter for the DIY XIAO ESP32-C6 water flow meter.
// Copy to <z2m data dir>/external_converters/water_flow_meter.mjs and restart Zigbee2MQTT.
//
// Endpoints (see firmware/flow_meter/flow_meter.ino):
//   1  genAnalogInput                 flow, L/min
//   2  genAnalogInput + genAnalogOutput   total water, L (write to align with your meter)
//   3  genAnalogOutput                calibration, pulses per litre
//   4  genAnalogInput                 backup battery voltage, V (optional)
//   10 temperature + humidity, 11 pressure (optional BME280)
import * as fz from 'zigbee-herdsman-converters/converters/fromZigbee';
import * as exposes from 'zigbee-herdsman-converters/lib/exposes';
import * as reporting from 'zigbee-herdsman-converters/lib/reporting';

const e = exposes.presets;
const ea = exposes.access;

const EP = {flow: 1, total: 2, calibration: 3, battery: 4, climate: 10, pressure: 11};
const BATTERY_EMPTY_V = 3.3;
const BATTERY_FULL_V = 4.2;

const round = (value, decimals) => Number(value.toFixed(decimals));

const batteryPercent = (volts) => {
    const percent = ((volts - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V)) * 100;
    return Math.round(Math.min(100, Math.max(0, percent)));
};

const fzAnalogInput = {
    cluster: 'genAnalogInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const value = msg.data.presentValue;
        if (typeof value !== 'number') return;
        switch (msg.endpoint.ID) {
            case EP.flow:
                return {flow: round(Math.max(0, value), 2)};
            case EP.total:
                return {water_consumed: round(value, 3)};
            case EP.battery:
                return {battery_voltage: round(value, 2), battery: batteryPercent(value)};
        }
    },
};

const fzAnalogOutput = {
    cluster: 'genAnalogOutput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const value = msg.data.presentValue;
        if (typeof value === 'number' && msg.endpoint.ID === EP.calibration) {
            return {pulses_per_liter: round(value, 1)};
        }
    },
};

const tzAnalogOutput = {
    key: ['pulses_per_liter', 'water_consumed'],
    convertSet: async (entity, key, value, meta) => {
        const number = Number(value);
        if (!Number.isFinite(number)) throw new Error(`${key} must be a number`);
        if (key === 'pulses_per_liter' && (number < 1 || number > 20000)) {
            throw new Error('pulses_per_liter must be between 1 and 20000');
        }
        if (key === 'water_consumed' && number < 0) throw new Error('water_consumed must be >= 0');
        const endpoint = meta.device.getEndpoint(key === 'pulses_per_liter' ? EP.calibration : EP.total);
        await endpoint.write('genAnalogOutput', {presentValue: number});
        return {state: {[key]: number}};
    },
    convertGet: async (entity, key, meta) => {
        if (key === 'pulses_per_liter') {
            await meta.device.getEndpoint(EP.calibration).read('genAnalogOutput', ['presentValue']);
        } else {
            await meta.device.getEndpoint(EP.total).read('genAnalogInput', ['presentValue']);
        }
    },
};

export default {
    zigbeeModel: ['C6WaterFlowMeter'],
    model: 'C6WaterFlowMeter',
    vendor: 'DIY',
    description: 'Water flow meter with Hall sensor, USB powered Zigbee router (XIAO ESP32-C6)',
    fromZigbee: [fzAnalogInput, fzAnalogOutput, fz.temperature, fz.humidity, fz.pressure],
    toZigbee: [tzAnalogOutput],
    exposes: [
        e.numeric('flow', ea.STATE_GET).withUnit('L/min').withDescription('Current water flow'),
        e
            .numeric('water_consumed', ea.STATE_GET)
            .withUnit('L')
            .withDescription('Total water since installation. Publish a value to /set to align it with your water meter'),
        e
            .numeric('pulses_per_liter', ea.ALL)
            .withValueMin(1)
            .withValueMax(20000)
            .withValueStep(1)
            .withDescription('Sensor calibration: pulses sent by the Hall sensor for one litre')
            .withCategory('config'),
        e.numeric('battery_voltage', ea.STATE).withUnit('V').withDescription('Backup 18650 voltage (if wired)').withCategory('diagnostic'),
        e.battery().withDescription('Backup 18650 charge, estimated from its voltage (if wired)'),
        e.temperature(),
        e.humidity(),
        e.pressure(),
    ],
    configure: async (device, coordinatorEndpoint) => {
        // The firmware sends its reports to the bound coordinator: bind what exists.
        const bindings = [
            [EP.flow, ['genAnalogInput']],
            [EP.total, ['genAnalogInput']],
            [EP.battery, ['genAnalogInput']],
            [EP.climate, ['msTemperatureMeasurement', 'msRelativeHumidity']],
            [EP.pressure, ['msPressureMeasurement']],
        ];
        for (const [id, clusters] of bindings) {
            const endpoint = device.getEndpoint(id);
            if (endpoint) await reporting.bind(endpoint, coordinatorEndpoint, clusters);
        }
        for (const id of [EP.flow, EP.total, EP.battery]) {
            const endpoint = device.getEndpoint(id);
            if (endpoint) await endpoint.read('genAnalogInput', ['presentValue']);
        }
        await device.getEndpoint(EP.calibration)?.read('genAnalogOutput', ['presentValue']);
    },
};
