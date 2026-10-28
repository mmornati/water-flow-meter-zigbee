// Integration tests: load the external converter through zigbee-herdsman-converters,
// the same library Zigbee2MQTT uses, and check what Z2M will see and send.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import * as zhc from 'zigbee-herdsman-converters';

import converter from '../water_flow_meter.mjs';

// Must match the firmware (firmware/flow_meter/flow_meter.ino)
const FIRMWARE_MANUFACTURER = 'DIY';
const FIRMWARE_MODEL = 'C6WaterFlowMeter';
const sketch = readFileSync(new URL('../../firmware/flow_meter/flow_meter.ino', import.meta.url), 'utf8');

const definition = zhc.prepareDefinition({...converter});

function exposes() {
    const list = typeof definition.exposes === 'function' ? definition.exposes(undefined, {}) : definition.exposes;
    return list.filter((e) => e.name !== 'linkquality');
}

function expose(property) {
    const found = exposes().find((e) => e.property === property);
    assert.ok(found, `${property} not exposed`);
    return found;
}

async function decode(cluster, endpointId, data, type = 'attributeReport') {
    const device = {modelID: FIRMWARE_MODEL, manufacturerName: FIRMWARE_MANUFACTURER};
    const meta = {device, logger: console, state: {}};
    const msg = {type, cluster, data, endpoint: {ID: endpointId}, device};
    const result = {};
    for (const fz of definition.fromZigbee) {
        const types = Array.isArray(fz.type) ? fz.type : [fz.type];
        if (fz.cluster === cluster && types.includes(type)) {
            Object.assign(result, await fz.convert(definition, msg, () => {}, {}, meta));
        }
    }
    return result;
}

function fakeDevice(endpointIds) {
    const calls = [];
    const endpoints = new Map(
        endpointIds.map((ID) => [
            ID,
            {
                ID,
                write: async (cluster, payload) => calls.push({op: 'write', ID, cluster, payload}),
                read: async (cluster, attributes) => calls.push({op: 'read', ID, cluster, attributes}),
                bind: async (cluster) => calls.push({op: 'bind', ID, cluster}),
                configureReporting: async () => {},
            },
        ]),
    );
    return {calls, getEndpoint: (ID) => endpoints.get(ID)};
}

function tz(key) {
    const found = definition.toZigbee.find((c) => c.key.includes(key));
    assert.ok(found, `no toZigbee converter for ${key}`);
    return found;
}

function firmwareDefine(name) {
    const match = sketch.match(new RegExp(`#define\\s+${name}\\s+(\\d+)`));
    assert.ok(match, `${name} not defined in the sketch`);
    return Number(match[1]);
}

test('firmware source still sends the expected manufacturer and model', () => {
    const match = sketch.match(/setManufacturerAndModel\("([^"]+)",\s*"([^"]+)"\)/);
    assert.ok(match, 'setManufacturerAndModel() call not found in the sketch');
    assert.equal(match[1], FIRMWARE_MANUFACTURER);
    assert.equal(match[2], FIRMWARE_MODEL);
});

test('endpoint numbers match the firmware', () => {
    assert.equal(firmwareDefine('EP_FLOW'), 1);
    assert.equal(firmwareDefine('EP_TOTAL'), 2);
    assert.equal(firmwareDefine('EP_CALIBRATION'), 3);
    assert.equal(firmwareDefine('EP_BATTERY'), 4);
    assert.equal(firmwareDefine('EP_CLIMATE'), 10);
    assert.equal(firmwareDefine('EP_PRESSURE'), 11);
});

test('matches the model string sent by the firmware', () => {
    assert.deepEqual(converter.zigbeeModel, [FIRMWARE_MODEL]);
    assert.equal(converter.model, FIRMWARE_MODEL);
    assert.equal(converter.vendor, FIRMWARE_MANUFACTURER);
});

test('Zigbee2MQTT resolves a device reporting the firmware model', async () => {
    zhc.addExternalDefinition({...converter, externalConverterName: 'water_flow_meter.mjs'});
    try {
        const device = {
            ieeeAddr: '0x0000000000000001',
            modelID: FIRMWARE_MODEL,
            manufacturerName: FIRMWARE_MANUFACTURER,
            type: 'Router',
            powerSource: 'Mains (single phase)',
            endpoints: [],
            interviewing: false,
            interviewCompleted: true,
            getEndpoint: () => undefined,
        };
        const found = await zhc.findByDevice(device);
        assert.ok(found, 'definition not found');
        assert.equal(found.model, FIRMWARE_MODEL);
        assert.equal(found.vendor, FIRMWARE_MANUFACTURER);
    } finally {
        zhc.removeExternalDefinitions('water_flow_meter.mjs');
    }
});

test('exposes the properties Home Assistant discovery knows about', () => {
    // Z2M's HA discovery maps these names: flow -> volume_flow_rate,
    // water_consumed -> water / total_increasing (Energy dashboard).
    const names = exposes().map((e) => e.property).sort();
    assert.deepEqual(names, ['battery', 'battery_voltage', 'flow', 'humidity', 'pressure', 'pulses_per_liter', 'temperature', 'water_consumed']);
    assert.equal(expose('flow').unit, 'L/min');
    assert.equal(expose('water_consumed').unit, 'L');
});

test('water_consumed is a read-only sensor, so HA does not turn it into a number entity', () => {
    const access = expose('water_consumed').access;
    assert.equal(access & 0b010, 0, 'water_consumed must not expose SET');
});

test('pulses_per_liter is a writable config number', () => {
    const calibration = expose('pulses_per_liter');
    assert.equal(calibration.access, 0b111);
    assert.equal(calibration.category, 'config');
    assert.equal(calibration.value_min, 1);
    assert.equal(calibration.value_max, 20000);
});

test('endpoint 1 analog input -> flow in L/min', async () => {
    assert.deepEqual(await decode('genAnalogInput', 1, {presentValue: 6.12345}), {flow: 6.12});
    assert.deepEqual(await decode('genAnalogInput', 1, {presentValue: 0}, 'readResponse'), {flow: 0});
});

test('endpoint 2 analog input -> water_consumed in L', async () => {
    assert.deepEqual(await decode('genAnalogInput', 2, {presentValue: 123456.5}), {water_consumed: 123456.5});
});

test('endpoint 4 analog input -> battery voltage and percentage', async () => {
    assert.deepEqual(await decode('genAnalogInput', 4, {presentValue: 4.2}), {battery_voltage: 4.2, battery: 100});
    assert.deepEqual(await decode('genAnalogInput', 4, {presentValue: 3.75}), {battery_voltage: 3.75, battery: 50});
    assert.deepEqual(await decode('genAnalogInput', 4, {presentValue: 3.0}), {battery_voltage: 3, battery: 0});
});

test('endpoint 3 analog output -> pulses_per_liter', async () => {
    assert.deepEqual(await decode('genAnalogOutput', 3, {presentValue: 490}, 'readResponse'), {pulses_per_liter: 490});
    // The "set total" output on endpoint 2 is write-only: ignored.
    assert.deepEqual(await decode('genAnalogOutput', 2, {presentValue: 5}, 'readResponse'), {});
});

test('BME280 clusters decode with the standard converters', async () => {
    assert.equal((await decode('msTemperatureMeasurement', 10, {measuredValue: 2150})).temperature, 21.5);
    assert.equal((await decode('msRelativeHumidity', 10, {measuredValue: 4520})).humidity, 45.2);
    assert.equal((await decode('msPressureMeasurement', 11, {measuredValue: 1013})).pressure, 1013);
});

test('setting pulses_per_liter writes the analog output on endpoint 3', async () => {
    const device = fakeDevice([1, 2, 3]);
    const result = await tz('pulses_per_liter').convertSet(device.getEndpoint(1), 'pulses_per_liter', 450, {device, state: {}, message: {}});
    assert.deepEqual(device.calls, [{op: 'write', ID: 3, cluster: 'genAnalogOutput', payload: {presentValue: 450}}]);
    assert.deepEqual(result, {state: {pulses_per_liter: 450}});
});

test('setting water_consumed writes the analog output on endpoint 2', async () => {
    const device = fakeDevice([1, 2, 3]);
    await tz('water_consumed').convertSet(device.getEndpoint(1), 'water_consumed', '250000', {device, state: {}, message: {}});
    assert.deepEqual(device.calls, [{op: 'write', ID: 2, cluster: 'genAnalogOutput', payload: {presentValue: 250000}}]);
});

test('invalid values are rejected before reaching the device', async () => {
    const device = fakeDevice([1, 2, 3]);
    const meta = {device, state: {}, message: {}};
    await assert.rejects(tz('pulses_per_liter').convertSet(null, 'pulses_per_liter', 0, meta));
    await assert.rejects(tz('pulses_per_liter').convertSet(null, 'pulses_per_liter', 'abc', meta));
    await assert.rejects(tz('water_consumed').convertSet(null, 'water_consumed', -1, meta));
    assert.equal(device.calls.length, 0);
});

test('configure binds only the endpoints the device has', async () => {
    // No battery divider, no BME280: endpoints 4, 10 and 11 are missing.
    const device = fakeDevice([1, 2, 3]);
    await definition.configure(device, {ID: 1}, definition);
    const binds = device.calls.filter((c) => c.op === 'bind').map((c) => `${c.ID}:${c.cluster}`);
    assert.deepEqual(binds, ['1:genAnalogInput', '2:genAnalogInput']);
    const reads = device.calls.filter((c) => c.op === 'read').map((c) => `${c.ID}:${c.cluster}`);
    assert.deepEqual(reads, ['1:genAnalogInput', '2:genAnalogInput', '3:genAnalogOutput']);
});

test('configure binds the optional sensors when present', async () => {
    const device = fakeDevice([1, 2, 3, 4, 10, 11]);
    await definition.configure(device, {ID: 1}, definition);
    const binds = device.calls.filter((c) => c.op === 'bind').map((c) => `${c.ID}:${c.cluster}`);
    assert.deepEqual(binds, [
        '1:genAnalogInput',
        '2:genAnalogInput',
        '4:genAnalogInput',
        '10:msTemperatureMeasurement',
        '10:msRelativeHumidity',
        '11:msPressureMeasurement',
    ]);
});
