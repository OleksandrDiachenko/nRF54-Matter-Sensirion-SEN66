/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "measurement_policy.h"

#include <errno.h>

#include <zephyr/ztest.h>

using namespace MeasurementService;

namespace {

constexpr uint16_t kAllValid = Sen66::kFieldPm1p0 | Sen66::kFieldPm2p5 | Sen66::kFieldPm4p0 |
                               Sen66::kFieldPm10 | Sen66::kFieldHumidity |
                               Sen66::kFieldTemperature | Sen66::kFieldVocIndex |
                               Sen66::kFieldNoxIndex | Sen66::kFieldCo2;

/* A plausible, fully-valid frame; individual tests override one field. */
Sen66::Measurement PlausibleMeasurement(uint16_t valid) {
    Sen66::Measurement m{};
    m.pm1p0 = 100;
    m.pm2p5 = 150;
    m.pm4p0 = 180;
    m.pm10 = 200;
    m.humidity = 4550;
    m.temperature = 4400;
    m.voc_index = 120;
    m.nox_index = 10;
    m.co2 = 800;
    m.valid = valid;
    return m;
}

} // namespace

ZTEST_SUITE(measurement_policy, NULL, NULL, NULL, NULL, NULL);

ZTEST(measurement_policy, test_steady_success_updates_snapshot_and_cadence) {
    Policy policy;
    const Sen66::Measurement m = PlausibleMeasurement(kAllValid);

    policy.OnReadResult(0, m, 1000);

    zassert_false(policy.NeedsRestart(), "a clean success must not enter backoff");
    zassert_equal(policy.NextDelayMs(), PolicyConfig{}.pollIntervalMs,
                  "steady state polls at the normal cadence");

    Sen66::Measurement latest;
    int64_t ageMs = 0;
    zassert_true(policy.GetLatest(latest, ageMs, 1500), "a valid frame was observed");
    zassert_equal(ageMs, 500, NULL);
    zassert_equal(latest.pm1p0, 100, NULL);
}

ZTEST(measurement_policy, test_getlatest_false_until_first_valid_frame) {
    Policy policy;
    Sen66::Measurement latest;
    int64_t ageMs = 0;

    zassert_false(policy.GetLatest(latest, ageMs, 0), "nothing has ever been read");

    policy.OnReadResult(0, PlausibleMeasurement(kAllValid), 100);
    zassert_true(policy.GetLatest(latest, ageMs, 100), NULL);
}

ZTEST(measurement_policy, test_warmup_frames_are_not_counted_as_failures) {
    Policy policy;
    const Sen66::Measurement stillWarming = PlausibleMeasurement(0); // transport ok, no channel valid yet

    for (int64_t t = 0; t < 5000; t += 1000) {
        policy.OnReadResult(0, stillWarming, t);
    }

    zassert_false(policy.NeedsRestart(), "warm-up ticks (err==0, valid==0) are not failures");
    zassert_equal(policy.NextDelayMs(), PolicyConfig{}.pollIntervalMs, NULL);

    Sen66::Measurement latest;
    int64_t ageMs = 0;
    zassert_false(policy.GetLatest(latest, ageMs, 5000), "still no channel has ever been valid");
}

ZTEST(measurement_policy, test_transient_failure_below_threshold_recovers_without_backoff) {
    PolicyConfig cfg;
    cfg.failureThreshold = 3;
    Policy policy(cfg);
    const Sen66::Measurement empty{};

    policy.OnReadResult(-EIO, empty, 0);
    policy.OnReadResult(-EIO, empty, 1000);
    zassert_false(policy.NeedsRestart(), "two failures stay below the threshold of three");
    zassert_equal(policy.NextDelayMs(), cfg.pollIntervalMs, NULL);

    policy.OnReadResult(0, PlausibleMeasurement(kAllValid), 2000);
    zassert_false(policy.NeedsRestart(), "a success before the threshold clears the counter");
}

ZTEST(measurement_policy, test_sustained_failure_enters_and_escalates_backoff) {
    PolicyConfig cfg;
    cfg.failureThreshold = 3;
    Policy policy(cfg);
    const Sen66::Measurement empty{};

    policy.OnReadResult(-EIO, empty, 0);
    policy.OnReadResult(-EIO, empty, 1000);
    policy.OnReadResult(-EIO, empty, 2000); // crosses the threshold
    zassert_true(policy.NeedsRestart(), "sustained failure must trigger a restart+backoff");
    zassert_equal(policy.NextDelayMs(), cfg.backoffStepsMs[0], NULL);

    policy.OnReadResult(-EIO, empty, 4000);
    zassert_equal(policy.NextDelayMs(), cfg.backoffStepsMs[1], "backoff escalates on repeat failure");

    policy.OnReadResult(-EIO, empty, 9000);
    zassert_equal(policy.NextDelayMs(), cfg.backoffStepsMs[2], NULL);

    policy.OnReadResult(-EIO, empty, 19000);
    zassert_equal(policy.NextDelayMs(), cfg.backoffStepsMs[3], NULL);

    // Further failures stay capped at the last step, never growing unbounded.
    policy.OnReadResult(-EIO, empty, 49000);
    zassert_equal(policy.NextDelayMs(), cfg.backoffStepsMs[3], "backoff must cap, not keep growing");
}

ZTEST(measurement_policy, test_recovery_after_backoff_resets_cadence) {
    PolicyConfig cfg;
    cfg.failureThreshold = 3;
    Policy policy(cfg);
    const Sen66::Measurement empty{};

    policy.OnReadResult(-EIO, empty, 0);
    policy.OnReadResult(-EIO, empty, 1000);
    policy.OnReadResult(-EIO, empty, 2000);
    zassert_true(policy.NeedsRestart(), NULL);

    policy.OnReadResult(0, PlausibleMeasurement(kAllValid), 4000);
    zassert_false(policy.NeedsRestart(), "a success after backoff must fully recover");
    zassert_equal(policy.NextDelayMs(), cfg.pollIntervalMs, "cadence returns to normal");
}

ZTEST(measurement_policy, test_first_valid_frame_notifies) {
    Policy policy;
    policy.OnReadResult(0, PlausibleMeasurement(kAllValid), 0);
    zassert_true(policy.ShouldNotify(), "data appearing for the first time is notify-worthy");
}

ZTEST(measurement_policy, test_validity_transition_notifies_immediately) {
    Policy policy;
    const Sen66::Measurement full = PlausibleMeasurement(kAllValid);
    policy.OnReadResult(0, full, 0);
    zassert_true(policy.ShouldNotify(), NULL);
    policy.MarkNotified(0);

    Sen66::Measurement dropped = full;
    dropped.valid = static_cast<uint16_t>(dropped.valid & ~static_cast<uint16_t>(Sen66::kFieldPm1p0));
    policy.OnReadResult(0, dropped, 500); // well inside the notify interval

    zassert_true(policy.ShouldNotify(), "a channel losing validity must notify immediately");
    zassert_true(policy.ChangedMask() & Sen66::kFieldPm1p0, "the flipped channel must be reported");
}

ZTEST(measurement_policy, test_small_change_waits_for_the_interval_heartbeat) {
    PolicyConfig cfg;
    cfg.notifyIntervalMs = 5000;
    Policy policy(cfg);

    const Sen66::Measurement base = PlausibleMeasurement(kAllValid);
    policy.OnReadResult(0, base, 0);
    zassert_true(policy.ShouldNotify(), NULL);
    policy.MarkNotified(0);

    Sen66::Measurement tiny = base;
    tiny.pm2p5 = static_cast<uint16_t>(tiny.pm2p5 + 1); // well under the pm threshold

    policy.OnReadResult(0, tiny, 2000); // before the interval elapses
    zassert_false(policy.ShouldNotify(), "a small delta before the interval must not notify");

    policy.OnReadResult(0, tiny, 5001); // interval elapsed since the last notify at t=0
    zassert_true(policy.ShouldNotify(), "the interval alone must trigger a heartbeat notify");
    zassert_equal(policy.ChangedMask(), 0, "a pure heartbeat reports no channel change");
}

ZTEST(measurement_policy, test_large_change_notifies_immediately) {
    PolicyConfig cfg;
    cfg.notifyIntervalMs = 5000;
    Policy policy(cfg);

    const Sen66::Measurement base = PlausibleMeasurement(kAllValid);
    policy.OnReadResult(0, base, 0);
    policy.MarkNotified(0);

    Sen66::Measurement changed = base;
    changed.pm2p5 = static_cast<uint16_t>(changed.pm2p5 + cfg.thresholds.pm); // at the threshold

    policy.OnReadResult(0, changed, 500); // well before the interval
    zassert_true(policy.ShouldNotify(), "a significant delta must notify immediately");
    zassert_true(policy.ChangedMask() & Sen66::kFieldPm2p5, NULL);
}
