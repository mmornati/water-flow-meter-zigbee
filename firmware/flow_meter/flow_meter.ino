/**
 * Water flow meter — Seeed XIAO ESP32-C6, USB powered with an 18650 backup, Zigbee router.
 * Counts the pulses of a Hall flow sensor (G3/4, 1-30 L/min) and reports to Zigbee2MQTT:
 *
 *   EP 1  Analog Input            flow, L/min
 *   EP 2  Analog Input + Output   total water, L (write the output to align it with your meter)
 *   EP 3  Analog Output           calibration, pulses per litre (stored in flash)
 *   EP 4  Analog Input            backup battery voltage, V      (only if BATTERY_ADC_PIN >= 0)
 *   EP 10 Temperature + Humidity  BME280                         (only if a BME280 answers)
 *   EP 11 Pressure                BME280                         (only if a BME280 answers)
 *
 * Pulses are counted by the PCNT hardware counter, so none are missed while the radio
 * is busy, and water keeps being counted (and saved) even when the network is down.
 *
 * PlatformIO: all settings are in ../platformio.ini.
 * Arduino IDE, Tools menu:
 *   Board:            XIAO_ESP32C6
 *   Zigbee mode:      Zigbee ZCZR (coordinator/router)
 *   Partition Scheme: Zigbee ZCZR 4MB with spiffs
 *   Erase All Flash Before Sketch Upload: Enabled (first flash only)
 * Library: "Adafruit BME280 Library" (Library Manager).
 *
 * Requires arduino-esp32 core 3.3.0 or newer.
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ZCZR
#error "Select Tools -> Zigbee mode -> Zigbee ZCZR (coordinator/router)"
#endif

#include "Zigbee.h"
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "flow_logic.h"

/* ---------- Pins (XIAO ESP32-C6) ---------- */
#define FLOW_PIN        2    // D2 = GPIO2, Hall sensor signal (yellow wire)
#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN -1   // set to 0 (D0/A0) once the battery divider is wired
#endif
#define I2C_SDA_PIN     17   // D7, BME280 SDA
#define I2C_SCL_PIN     19   // D8, BME280 SCL
#define LED_PIN         15   // user LED, active LOW
#define BUTTON_PIN      BOOT_PIN  // GPIO9, BOOT button
#define RF_SWITCH_EN    3    // LOW = enable RF switch control
#define RF_ANT_SELECT   14   // HIGH = external U.FL antenna, LOW = ceramic

/* ---------- Endpoints ---------- */
#define EP_FLOW         1
#define EP_TOTAL        2
#define EP_CALIBRATION  3
#define EP_BATTERY      4
#define EP_CLIMATE      10
#define EP_PRESSURE     11

/* ---------- Behaviour ---------- */
#define DEFAULT_PULSES_PER_LITER 490.0f  // calibrate: see docs/03-firmware.md
#define MAX_PULSES_PER_LITER     20000.0f
#define SAMPLE_MS                1000     // flow is averaged over FlowMeter::WINDOW samples
#define REPORT_MIN_INTERVAL_MS   5000     // at most one flow/total report every 5 s
#define HEARTBEAT_MS             (5UL * 60UL * 1000UL)   // re-send everything every 5 min
#define FLOW_REPORT_DELTA        0.1f     // L/min
#define TOTAL_REPORT_DELTA       0.5      // L
#define SAVE_MAX_INTERVAL_MS     (10UL * 60UL * 1000UL)  // flash write while water keeps flowing
#define BATTERY_REPORT_MS        (60UL * 1000UL)
#define BATTERY_DIVIDER          2.0f     // two equal resistors: battery+ -> A0 -> GND
#define CLIMATE_REPORT_MS        (5UL * 60UL * 1000UL)
#define PCNT_HIGH_LIMIT          30000
#define PCNT_GLITCH_NS           10000    // ignore spikes shorter than 10 µs
#define FACTORY_RESET_HOLD_MS    3000

ZigbeeAnalog zbFlow = ZigbeeAnalog(EP_FLOW);
ZigbeeAnalog zbTotal = ZigbeeAnalog(EP_TOTAL);
ZigbeeAnalog zbCalibration = ZigbeeAnalog(EP_CALIBRATION);
ZigbeeAnalog zbBattery = ZigbeeAnalog(EP_BATTERY);
ZigbeeTempSensor zbClimate = ZigbeeTempSensor(EP_CLIMATE);
ZigbeePressureSensor zbPressure = ZigbeePressureSensor(EP_PRESSURE);

FlowMeter meter(DEFAULT_PULSES_PER_LITER);
FlowReporter reporter(REPORT_MIN_INTERVAL_MS, HEARTBEAT_MS, FLOW_REPORT_DELTA, TOTAL_REPORT_DELTA);
TotalSaver saver(SAVE_MAX_INTERVAL_MS);
Preferences preferences;
Adafruit_BME280 bme;
pcnt_unit_handle_t pcntUnit = nullptr;

bool bmeFound = false;
bool wasConnected = false;
int lastPulseCount = 0;
unsigned long lastSampleMs = 0;
unsigned long lastBatteryReportMs = 0;
unsigned long lastClimateReportMs = 0;

// Written by the Zigbee task, applied in loop().
volatile bool pulsesPerLiterPending = false;
volatile float pendingPulsesPerLiter = 0;
volatile bool totalPending = false;
volatile float pendingTotal = 0;

static inline void ledOn()  { digitalWrite(LED_PIN, LOW); }
static inline void ledOff() { digitalWrite(LED_PIN, HIGH); }

void useExternalAntenna() {
  pinMode(RF_SWITCH_EN, OUTPUT);
  digitalWrite(RF_SWITCH_EN, LOW);
  delay(100);
  pinMode(RF_ANT_SELECT, OUTPUT);
  digitalWrite(RF_ANT_SELECT, HIGH);
}

bool setupPulseCounter() {
  pcnt_unit_config_t unitConfig = {};
  unitConfig.low_limit = -PCNT_HIGH_LIMIT;
  unitConfig.high_limit = PCNT_HIGH_LIMIT;
  unitConfig.flags.accum_count = 1;  // keep counting past the 16-bit hardware limit
  if (pcnt_new_unit(&unitConfig, &pcntUnit) != ESP_OK) {
    return false;
  }

  pcnt_glitch_filter_config_t filterConfig = {};
  filterConfig.max_glitch_ns = PCNT_GLITCH_NS;
  pcnt_unit_set_glitch_filter(pcntUnit, &filterConfig);

  pcnt_chan_config_t channelConfig = {};
  channelConfig.edge_gpio_num = FLOW_PIN;
  channelConfig.level_gpio_num = -1;
  pcnt_channel_handle_t channel = nullptr;
  if (pcnt_new_channel(pcntUnit, &channelConfig, &channel) != ESP_OK) {
    return false;
  }
  // One pulse = one rising edge, like ESPHome's pulse_counter default.
  pcnt_channel_set_edge_action(channel, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
  pcnt_unit_add_watch_point(pcntUnit, PCNT_HIGH_LIMIT);  // needed by accum_count
  gpio_pullup_en((gpio_num_t)FLOW_PIN);  // an external pull-up is still recommended

  return pcnt_unit_enable(pcntUnit) == ESP_OK && pcnt_unit_clear_count(pcntUnit) == ESP_OK && pcnt_unit_start(pcntUnit) == ESP_OK;
}

uint32_t takePulses() {
  int count = 0;
  pcnt_unit_get_count(pcntUnit, &count);
  uint32_t pulses = (uint32_t)count - (uint32_t)lastPulseCount;
  lastPulseCount = count;
  return pulses;
}

void loadSettings() {
  preferences.begin("flowmeter", true);
  float pulsesPerLiter = preferences.getFloat("ppl", DEFAULT_PULSES_PER_LITER);
  double total = preferences.getDouble("total", 0.0);
  preferences.end();

  meter.setPulsesPerLiter(pulsesPerLiter);
  meter.setTotalLiters(total);
  Serial.printf("Calibration %.1f pulses/L, total %.3f L\n", meter.pulsesPerLiter(), meter.totalLiters());
}

void saveTotal() {
  preferences.begin("flowmeter", false);
  preferences.putDouble("total", meter.totalLiters());
  preferences.end();
  saver.saved(meter.totalLiters(), millis());
  Serial.printf("Total saved: %.3f L\n", meter.totalLiters());
}

void savePulsesPerLiter() {
  preferences.begin("flowmeter", false);
  preferences.putFloat("ppl", meter.pulsesPerLiter());
  preferences.end();
}

void onPulsesPerLiterChange(float pulsesPerLiter) {
  pendingPulsesPerLiter = pulsesPerLiter;
  pulsesPerLiterPending = true;
}

void onTotalSet(float liters) {
  pendingTotal = liters;
  totalPending = true;
}

void sendFlowAndTotal() {
  float flow = meter.flowLitersPerMinute();
  float total = (float)meter.totalLiters();
  zbFlow.setAnalogInput(flow);
  zbTotal.setAnalogInput(total);
  if (Zigbee.connected()) {
    zbFlow.reportAnalogInput();
    zbTotal.reportAnalogInput();
  }
  Serial.printf("Flow %.2f L/min, total %.3f L\n", flow, meter.totalLiters());
}

float readBatteryVolts() {
#if BATTERY_ADC_PIN >= 0
  uint32_t millivolts = 0;
  for (int i = 0; i < 16; i++) {
    millivolts += analogReadMilliVolts(BATTERY_ADC_PIN);
  }
  return millivolts / 16.0f / 1000.0f * BATTERY_DIVIDER;
#else
  return 0.0f;
#endif
}

void sendBattery() {
  float volts = readBatteryVolts();
  zbBattery.setAnalogInput(volts);
  zbBattery.reportAnalogInput();
  Serial.printf("Battery %.2f V\n", volts);
}

void sendClimate() {
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressureHpa = bme.readPressure() / 100.0f;
  if (isnan(temperature) || isnan(humidity) || isnan(pressureHpa)) {
    Serial.println("BME280 read failed");
    return;
  }
  zbClimate.setTemperature(temperature);
  zbClimate.setHumidity(humidity);
  zbClimate.report();
  zbPressure.setPressure((int16_t)lroundf(pressureHpa));
  zbPressure.report();
  Serial.printf("BME280 %.2f C, %.1f %%, %.0f hPa\n", temperature, humidity, pressureHpa);
}

void applyRemoteChanges() {
  if (pulsesPerLiterPending) {
    pulsesPerLiterPending = false;
    float pulsesPerLiter = pendingPulsesPerLiter;
    if (pulsesPerLiter <= MAX_PULSES_PER_LITER && meter.setPulsesPerLiter(pulsesPerLiter)) {
      savePulsesPerLiter();
      Serial.printf("Calibration set to %.1f pulses/L\n", pulsesPerLiter);
    } else {
      Serial.printf("Calibration %.1f rejected\n", pulsesPerLiter);
    }
    zbCalibration.setAnalogOutput(meter.pulsesPerLiter());
    zbCalibration.reportAnalogOutput();
  }

  if (totalPending) {
    totalPending = false;
    meter.setTotalLiters(pendingTotal);
    saveTotal();
    reporter.invalidate();
    Serial.printf("Total set to %.3f L\n", meter.totalLiters());
  }
}

void checkFactoryReset() {
  if (digitalRead(BUTTON_PIN) != LOW) {
    return;
  }
  unsigned long start = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    // Fast blink while held so you know when to release.
    digitalWrite(LED_PIN, (millis() / 100) % 2);
    if (millis() - start > FACTORY_RESET_HOLD_MS) {
      // Leaves the network only: the total and the calibration stay in flash.
      Serial.println("Factory reset: leaving network and rebooting");
      saveTotal();
      ledOn();
      delay(1000);
      Zigbee.factoryReset();  // reboots
    }
  }
  ledOff();
}

void setupAnalogInput(ZigbeeAnalog &ep, const char *description, float resolution) {
  ep.addAnalogInput();
  ep.setAnalogInputDescription(description);
  ep.setAnalogInputResolution(resolution);
}

void setup() {
  Serial.begin(115200);

  useExternalAntenna();

  pinMode(LED_PIN, OUTPUT);
  ledOff();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  loadSettings();
  if (!setupPulseCounter()) {
    Serial.println("Pulse counter setup failed, rebooting");
    delay(1000);
    ESP.restart();
  }

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  bmeFound = bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire);
  Serial.println(bmeFound ? "BME280 found" : "No BME280, climate endpoints disabled");

  zbFlow.setManufacturerAndModel("DIY", "C6WaterFlowMeter");
  zbFlow.setPowerSource(ZB_POWER_SOURCE_MAINS);
  setupAnalogInput(zbFlow, "Flow L/min", 0.01f);
  Zigbee.addEndpoint(&zbFlow);

  setupAnalogInput(zbTotal, "Total L", 0.001f);
  zbTotal.addAnalogOutput();
  zbTotal.setAnalogOutputDescription("Set total L");
  zbTotal.setAnalogOutputMinMax(0, 1e8f);
  Zigbee.addEndpoint(&zbTotal);

  zbCalibration.addAnalogOutput();
  zbCalibration.setAnalogOutputDescription("Pulses per L");
  zbCalibration.setAnalogOutputMinMax(1, MAX_PULSES_PER_LITER);
  zbCalibration.setAnalogOutputResolution(1);
  Zigbee.addEndpoint(&zbCalibration);

#if BATTERY_ADC_PIN >= 0
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  setupAnalogInput(zbBattery, "Battery V", 0.01f);
  Zigbee.addEndpoint(&zbBattery);
#endif

  if (bmeFound) {
    zbClimate.setMinMaxValue(-40, 85);
    zbClimate.addHumiditySensor(0, 100, 1);
    Zigbee.addEndpoint(&zbClimate);
    zbPressure.setMinMaxValue(300, 1100);
    Zigbee.addEndpoint(&zbPressure);
  }

  Serial.println("Starting Zigbee (router)");
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    Serial.println("Zigbee failed to start, rebooting");
    delay(1000);
    ESP.restart();
  }

  // Publish the stored calibration before listening for remote changes.
  zbCalibration.setAnalogOutput(meter.pulsesPerLiter());
  zbCalibration.onAnalogOutputChange(onPulsesPerLiterChange);
  zbTotal.onAnalogOutputChange(onTotalSet);

  unsigned long now = millis();
  lastSampleMs = now;
  saver.begin(meter.totalLiters(), now);
  reporter.begin(meter.flowLitersPerMinute(), meter.totalLiters(), now);
  // Climate and battery are sent as soon as the network is joined.
  lastBatteryReportMs = now - BATTERY_REPORT_MS;
  lastClimateReportMs = now - CLIMATE_REPORT_MS;

  // Slow blink until joined (see loop). Open "Permit join" in Zigbee2MQTT now.
  Serial.println("Waiting for the network; water is counted meanwhile");
}

void loop() {
  unsigned long now = millis();
  bool connected = Zigbee.connected();

  if (now - lastSampleMs >= SAMPLE_MS) {
    meter.addSample(takePulses(), now - lastSampleMs);
    lastSampleMs = now;
  }

  applyRemoteChanges();

  if (connected && !wasConnected) {
    Serial.println("Joined the network");
    ledOff();
    zbCalibration.reportAnalogOutput();
    reporter.invalidate();
  } else if (!connected) {
    digitalWrite(LED_PIN, (now / 500) % 2);
  }
  wasConnected = connected;

  if (reporter.update(meter.flowLitersPerMinute(), meter.totalLiters(), now) != FlowReporter::NONE) {
    sendFlowAndTotal();
  }

  if (saver.shouldSave(meter.flowLitersPerMinute(), meter.totalLiters(), now)) {
    saveTotal();
  }

  if (connected) {
#if BATTERY_ADC_PIN >= 0
    if (now - lastBatteryReportMs >= BATTERY_REPORT_MS) {
      lastBatteryReportMs = now;
      sendBattery();
    }
#endif
    if (bmeFound && now - lastClimateReportMs >= CLIMATE_REPORT_MS) {
      lastClimateReportMs = now;
      sendClimate();
    }
  }

  checkFactoryReset();
  delay(20);
}
