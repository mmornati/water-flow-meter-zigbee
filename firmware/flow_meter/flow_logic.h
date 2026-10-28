#pragma once

#include <math.h>
#include <stdint.h>

/**
 * Hardware-free logic of the water flow meter, unit tested on the host (see firmware/test).
 *
 * - FlowMeter turns pulse counts into a flow rate (L/min) and a running total (L).
 * - FlowReporter decides when flow and total have to be sent to the Zigbee network.
 * - TotalSaver decides when the total has to be written to flash.
 *
 * Time arithmetic is unsigned, which keeps it correct when millis() wraps around
 * after ~49 days.
 */

/**
 * Converts pulses from the Hall sensor into litres.
 *
 * The flow is averaged over the last WINDOW samples (one sample per second in the
 * sketch), which smooths the reading without hiding a tap being closed for long.
 * The total is kept as a double, so it stays exact to the millilitre for decades.
 * Changing the calibration only affects pulses counted from then on: the litres
 * already counted are never rewritten.
 */
class FlowMeter {
public:
  static const uint8_t WINDOW = 5;

  explicit FlowMeter(float pulsesPerLiter, double totalLiters = 0.0)
    : _pulsesPerLiter(pulsesPerLiter), _totalLiters(totalLiters) {}

  // Adds the pulses counted during the last `elapsedMs` milliseconds.
  void addSample(uint32_t pulses, uint32_t elapsedMs) {
    _pulses[_next] = pulses;
    _elapsedMs[_next] = elapsedMs;
    _next = (_next + 1) % WINDOW;
    if (_count < WINDOW) {
      _count++;
    }
    if (_pulsesPerLiter > 0) {
      _totalLiters += pulses / (double)_pulsesPerLiter;
    }
  }

  // Average flow over the window, in litres per minute. 0 when nothing flows.
  float flowLitersPerMinute() const {
    if (_pulsesPerLiter <= 0) {
      return 0.0f;
    }
    uint32_t pulses = 0;
    uint32_t elapsedMs = 0;
    for (uint8_t i = 0; i < _count; i++) {
      pulses += _pulses[i];
      elapsedMs += _elapsedMs[i];
    }
    if (pulses == 0 || elapsedMs == 0) {
      return 0.0f;
    }
    return (pulses / _pulsesPerLiter) * (60000.0f / elapsedMs);
  }

  double totalLiters() const {
    return _totalLiters;
  }

  // Aligns the total with the main water meter reading (or resets it to 0).
  void setTotalLiters(double liters) {
    _totalLiters = liters < 0 ? 0.0 : liters;
  }

  float pulsesPerLiter() const {
    return _pulsesPerLiter;
  }

  // Ignored when not strictly positive.
  bool setPulsesPerLiter(float pulsesPerLiter) {
    if (!(pulsesPerLiter > 0)) {
      return false;
    }
    _pulsesPerLiter = pulsesPerLiter;
    return true;
  }

private:
  float _pulsesPerLiter;
  double _totalLiters;
  uint32_t _pulses[WINDOW] = {};
  uint32_t _elapsedMs[WINDOW] = {};
  uint8_t _next = 0;
  uint8_t _count = 0;
};

/**
 * Decides when flow and total are reported.
 *
 * - Water starting or stopping is reported right away (still no faster than `minIntervalMs`).
 * - While water flows, a report goes out when the flow moved by `flowDelta` L/min or the
 *   total by `totalDelta` L, at most once every `minIntervalMs`.
 * - Everything is re-sent every `heartbeatMs`, so a lost message never leaves the
 *   coordinator with a stale value.
 */
class FlowReporter {
public:
  enum Action : uint8_t {
    NONE = 0,
    REPORT_CHANGE,
    REPORT_HEARTBEAT,
  };

  FlowReporter(uint32_t minIntervalMs, uint32_t heartbeatMs, float flowDelta, double totalDelta)
    : _minIntervalMs(minIntervalMs), _heartbeatMs(heartbeatMs), _flowDelta(flowDelta), _totalDelta(totalDelta) {}

  // Records the initial values. The caller sends them right away.
  void begin(float flow, double total, uint32_t nowMs) {
    remember(flow, total, nowMs);
  }

  Action update(float flow, double total, uint32_t nowMs) {
    bool startedOrStopped = (flow > 0) != (_lastFlow > 0);
    bool changed = startedOrStopped || fabsf(flow - _lastFlow) >= _flowDelta || fabs(total - _lastTotal) >= _totalDelta;

    if (changed && nowMs - _lastReportMs >= _minIntervalMs) {
      remember(flow, total, nowMs);
      return REPORT_CHANGE;
    }

    if (nowMs - _lastReportMs >= _heartbeatMs) {
      remember(flow, total, nowMs);
      return REPORT_HEARTBEAT;
    }

    return NONE;
  }

  // Forces the next update() to report, e.g. after the total was set remotely.
  void invalidate() {
    _lastTotal = -INFINITY;
  }

private:
  void remember(float flow, double total, uint32_t nowMs) {
    _lastFlow = flow;
    _lastTotal = total;
    _lastReportMs = nowMs;
  }

  uint32_t _minIntervalMs;
  uint32_t _heartbeatMs;
  float _flowDelta;
  double _totalDelta;
  float _lastFlow = 0.0f;
  double _lastTotal = 0.0;
  uint32_t _lastReportMs = 0;
};

/**
 * Decides when the total is written to flash, to survive a reboot without wearing it out.
 *
 * - Once the water has stopped: one write per use of water (shower, washing machine...).
 * - Every `maxIntervalMs` while water keeps flowing (garden hose, leak), so a long
 *   flow is not lost if the board resets in the middle of it.
 * Nothing is written while the total does not change.
 */
class TotalSaver {
public:
  explicit TotalSaver(uint32_t maxIntervalMs) : _maxIntervalMs(maxIntervalMs) {}

  // Records the value already in flash.
  void begin(double savedTotal, uint32_t nowMs) {
    _savedTotal = savedTotal;
    _lastSaveMs = nowMs;
  }

  // true = write `total` now. The caller then calls saved().
  bool shouldSave(float flow, double total, uint32_t nowMs) const {
    if (total == _savedTotal) {
      return false;
    }
    return flow <= 0 || nowMs - _lastSaveMs >= _maxIntervalMs;
  }

  void saved(double total, uint32_t nowMs) {
    _savedTotal = total;
    _lastSaveMs = nowMs;
  }

private:
  uint32_t _maxIntervalMs;
  double _savedTotal = 0.0;
  uint32_t _lastSaveMs = 0;
};
