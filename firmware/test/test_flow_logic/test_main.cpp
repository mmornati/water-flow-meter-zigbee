// Host-side unit tests for flow_logic.h (run with: pio test -e native)
#include <unity.h>

#include "flow_logic.h"

static const float PPL = 490.0f;  // pulses per litre
static const uint32_t MIN_INTERVAL = 5000;
static const uint32_t HEARTBEAT = 300000;
static const uint32_t SAVE_INTERVAL = 600000;

void setUp() {}
void tearDown() {}

/* ---------- FlowMeter ---------- */

void test_no_pulses_no_flow() {
  FlowMeter meter(PPL);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, meter.flowLitersPerMinute());
  for (int i = 0; i < 10; i++) {
    meter.addSample(0, 1000);
  }
  TEST_ASSERT_EQUAL_FLOAT(0.0f, meter.flowLitersPerMinute());
  TEST_ASSERT_EQUAL_DOUBLE(0.0, meter.totalLiters());
}

void test_steady_flow_in_liters_per_minute() {
  FlowMeter meter(PPL);
  // 6 L/min = 0.1 L/s = 49 pulses per second
  for (int i = 0; i < FlowMeter::WINDOW; i++) {
    meter.addSample(49, 1000);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, meter.flowLitersPerMinute());
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, meter.totalLiters());
}

void test_flow_uses_real_elapsed_time() {
  FlowMeter meter(PPL);
  // A late sample (loop busy for 2 s) must not double the flow.
  meter.addSample(49, 1000);
  meter.addSample(98, 2000);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, meter.flowLitersPerMinute());
}

void test_flow_drops_to_zero_once_window_is_empty() {
  FlowMeter meter(PPL);
  for (int i = 0; i < FlowMeter::WINDOW; i++) {
    meter.addSample(49, 1000);
  }
  for (int i = 0; i < FlowMeter::WINDOW - 1; i++) {
    meter.addSample(0, 1000);
    TEST_ASSERT_TRUE(meter.flowLitersPerMinute() > 0.0f);
  }
  meter.addSample(0, 1000);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, meter.flowLitersPerMinute());
  // The total keeps what was counted.
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, meter.totalLiters());
}

void test_total_accumulates_exactly() {
  FlowMeter meter(PPL);
  // 1000 L, one pulse at a time: no drift.
  for (uint32_t i = 0; i < 490000; i++) {
    meter.addSample(1, 10);
  }
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1000.0, meter.totalLiters());
}

void test_total_starts_from_saved_value() {
  FlowMeter meter(PPL, 123456.789);
  meter.addSample(490, 1000);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 123457.789, meter.totalLiters());
}

void test_set_total_aligns_with_main_meter() {
  FlowMeter meter(PPL);
  meter.addSample(4900, 1000);
  meter.setTotalLiters(250000.0);
  TEST_ASSERT_EQUAL_DOUBLE(250000.0, meter.totalLiters());
  meter.setTotalLiters(-5.0);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, meter.totalLiters());
}

void test_calibration_change_does_not_rewrite_history() {
  FlowMeter meter(PPL);
  meter.addSample(490, 1000);  // 1 L at 490 pulses/L
  TEST_ASSERT_TRUE(meter.setPulsesPerLiter(245.0f));
  meter.addSample(245, 1000);  // 1 L at 245 pulses/L
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, meter.totalLiters());
}

void test_invalid_calibration_is_ignored() {
  FlowMeter meter(PPL);
  TEST_ASSERT_FALSE(meter.setPulsesPerLiter(0.0f));
  TEST_ASSERT_FALSE(meter.setPulsesPerLiter(-10.0f));
  TEST_ASSERT_FALSE(meter.setPulsesPerLiter(NAN));
  TEST_ASSERT_EQUAL_FLOAT(PPL, meter.pulsesPerLiter());
}

/* ---------- FlowReporter ---------- */

void test_reporter_quiet_when_nothing_changes() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  r.begin(0.0f, 100.0, 0);
  for (uint32_t t = 0; t < HEARTBEAT; t += 1000) {
    TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(0.0f, 100.0, t));
  }
}

void test_reporter_water_starting_is_reported() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  r.begin(0.0f, 100.0, 0);
  // Even a tiny flow (a dripping tap) is worth a report.
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_CHANGE, r.update(0.05f, 100.0, 10000));
}

void test_reporter_water_stopping_is_reported() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  r.begin(6.0f, 100.0, 0);
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_CHANGE, r.update(0.0f, 100.3, 10000));
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(0.0f, 100.3, 20000));
}

void test_reporter_respects_min_interval() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  r.begin(0.0f, 100.0, 0);
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_CHANGE, r.update(6.0f, 100.1, 10000));
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(9.0f, 101.0, 11000));
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(9.0f, 101.0, 14999));
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_CHANGE, r.update(9.0f, 101.0, 15000));
}

void test_reporter_ignores_small_flow_jitter() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  r.begin(6.0f, 100.0, 0);
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(6.05f, 100.2, 10000));
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_CHANGE, r.update(6.15f, 100.2, 11000));
}

void test_reporter_total_delta_triggers_report() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  r.begin(6.0f, 100.0, 0);
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(6.0f, 100.49, 10000));
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_CHANGE, r.update(6.0f, 100.5, 11000));
}

void test_reporter_heartbeat() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  r.begin(0.0f, 100.0, 0);
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(0.0f, 100.0, HEARTBEAT - 1));
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_HEARTBEAT, r.update(0.0f, 100.0, HEARTBEAT));
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(0.0f, 100.0, HEARTBEAT + 1));
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_HEARTBEAT, r.update(0.0f, 100.0, 2 * HEARTBEAT));
}

void test_reporter_invalidate_forces_report() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  r.begin(0.0f, 100.0, 0);
  r.invalidate();
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_CHANGE, r.update(0.0f, 100.0, MIN_INTERVAL));
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(0.0f, 100.0, 2 * MIN_INTERVAL));
}

void test_reporter_millis_rollover() {
  FlowReporter r(MIN_INTERVAL, HEARTBEAT, 0.1f, 0.5);
  const uint32_t nearWrap = 0xFFFFFFFFu - 1000;
  r.begin(0.0f, 100.0, nearWrap);
  TEST_ASSERT_EQUAL(FlowReporter::NONE, r.update(6.0f, 100.1, 2000));      // wrapped, 3001 ms later
  TEST_ASSERT_EQUAL(FlowReporter::REPORT_CHANGE, r.update(6.0f, 100.1, 4000));  // 5001 ms later
}

/* ---------- TotalSaver ---------- */

void test_saver_nothing_to_save() {
  TotalSaver s(SAVE_INTERVAL);
  s.begin(100.0, 0);
  TEST_ASSERT_FALSE(s.shouldSave(0.0f, 100.0, 10 * SAVE_INTERVAL));
}

void test_saver_saves_once_water_stops() {
  TotalSaver s(SAVE_INTERVAL);
  s.begin(100.0, 0);
  TEST_ASSERT_FALSE(s.shouldSave(6.0f, 105.0, 30000));  // still flowing
  TEST_ASSERT_TRUE(s.shouldSave(0.0f, 105.0, 40000));   // stopped
  s.saved(105.0, 40000);
  TEST_ASSERT_FALSE(s.shouldSave(0.0f, 105.0, 41000));
}

void test_saver_saves_periodically_during_long_flow() {
  TotalSaver s(SAVE_INTERVAL);
  s.begin(100.0, 0);
  TEST_ASSERT_FALSE(s.shouldSave(10.0f, 150.0, SAVE_INTERVAL - 1));
  TEST_ASSERT_TRUE(s.shouldSave(10.0f, 200.0, SAVE_INTERVAL));
  s.saved(200.0, SAVE_INTERVAL);
  TEST_ASSERT_FALSE(s.shouldSave(10.0f, 250.0, SAVE_INTERVAL + 1000));
}

void test_saver_millis_rollover() {
  TotalSaver s(SAVE_INTERVAL);
  const uint32_t nearWrap = 0xFFFFFFFFu - 1000;
  s.begin(100.0, nearWrap);
  TEST_ASSERT_FALSE(s.shouldSave(10.0f, 150.0, 5000));
  TEST_ASSERT_TRUE(s.shouldSave(10.0f, 150.0, SAVE_INTERVAL - 999));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_no_pulses_no_flow);
  RUN_TEST(test_steady_flow_in_liters_per_minute);
  RUN_TEST(test_flow_uses_real_elapsed_time);
  RUN_TEST(test_flow_drops_to_zero_once_window_is_empty);
  RUN_TEST(test_total_accumulates_exactly);
  RUN_TEST(test_total_starts_from_saved_value);
  RUN_TEST(test_set_total_aligns_with_main_meter);
  RUN_TEST(test_calibration_change_does_not_rewrite_history);
  RUN_TEST(test_invalid_calibration_is_ignored);
  RUN_TEST(test_reporter_quiet_when_nothing_changes);
  RUN_TEST(test_reporter_water_starting_is_reported);
  RUN_TEST(test_reporter_water_stopping_is_reported);
  RUN_TEST(test_reporter_respects_min_interval);
  RUN_TEST(test_reporter_ignores_small_flow_jitter);
  RUN_TEST(test_reporter_total_delta_triggers_report);
  RUN_TEST(test_reporter_heartbeat);
  RUN_TEST(test_reporter_invalidate_forces_report);
  RUN_TEST(test_reporter_millis_rollover);
  RUN_TEST(test_saver_nothing_to_save);
  RUN_TEST(test_saver_saves_once_water_stops);
  RUN_TEST(test_saver_saves_periodically_during_long_flow);
  RUN_TEST(test_saver_millis_rollover);
  return UNITY_END();
}
