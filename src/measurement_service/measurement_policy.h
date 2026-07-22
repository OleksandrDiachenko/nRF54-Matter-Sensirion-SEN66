/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "sen66/sen66_protocol.h"

#include <stddef.h>
#include <stdint.h>

namespace MeasurementService {

/*
 * Minimum raw-unit delta (driver's native fixed-point scale, see
 * sen66_protocol.h) on a channel, compared against the last notified
 * snapshot, that counts as a "significant change" worth an early publish.
 * A channel is only compared when it is valid in both snapshots.
 */
struct ChangeThresholds {
    uint16_t pm = 50;           // 5 ug/m^3 at raw/10 (pm1p0, pm2p5, pm4p0, pm10)
    uint16_t temperature = 100; // 0.5 degC at raw/200
    uint16_t humidity = 300;    // 3 %RH at raw/100
    uint16_t co2 = 50;          // 50 ppm at raw/1
    uint16_t index = 20;        // 2.0 index points at raw/10 (voc_index, nox_index)
};

struct PolicyConfig {
    uint32_t pollIntervalMs = 1000;
    uint32_t notifyIntervalMs = 7000;
    uint32_t failureThreshold = 3;
    /* Escalating wait between poll attempts once failureThreshold is reached;
     * the last entry is the sustained-fault cadence. */
    uint32_t backoffStepsMs[4] = {2000, 5000, 10000, 30000};
    ChangeThresholds thresholds{};
};

/*
 * Pure poll/retry/notify policy for the SEN66 measurement service. Has no
 * Zephyr dependency and performs no I/O: the caller feeds it the outcome of
 * each Sen66::ReadMeasurement() attempt and drives timing off NextDelayMs()
 * and NeedsRestart(). This mirrors the sen66_protocol split - decode/policy
 * logic stays host-testable, the I2C glue lives in measurement_service.cpp.
 */
class Policy {
  public:
    explicit Policy(const PolicyConfig &config = PolicyConfig{});

    /* Feed the outcome of one Sen66::ReadMeasurement() call. `err` is the
     * driver's return code (0 on success, negative errno on I2C/CRC failure).
     * `measurement` is only read when err == 0. */
    void OnReadResult(int err, const Sen66::Measurement &measurement, int64_t nowMs);

    /* Latest known-good snapshot. Returns false until the first successful
     * read with at least one valid channel has ever been observed. */
    bool GetLatest(Sen66::Measurement &out, int64_t &ageMs, int64_t nowMs) const;

    /* Whether the caller should invoke its update notification now. Valid
     * immediately after OnReadResult(); consumed by MarkNotified(). */
    bool ShouldNotify() const { return shouldNotify_; }

    /* Channels that changed significantly, or flipped validity, since the
     * last notification. Only meaningful when ShouldNotify() is true; a
     * pure interval heartbeat with no channel change reports 0. */
    uint16_t ChangedMask() const { return changedMask_; }

    /* Record that the pending notification was delivered. */
    void MarkNotified(int64_t nowMs);

    /* Delay before the next poll tick, honoring backoff. */
    uint32_t NextDelayMs() const;

    /* True when sustained failures mean the caller should reissue
     * Sen66::StartMeasurement() before the next Sen66::ReadMeasurement(). */
    bool NeedsRestart() const { return inBackoff_; }

  private:
    void EvaluateNotify(const Sen66::Measurement &fresh);

    PolicyConfig config_;

    Sen66::Measurement latest_{};
    bool everValid_ = false;
    int64_t lastUpdateMs_ = 0;

    Sen66::Measurement lastNotified_{};
    int64_t lastNotifyMs_ = 0;
    bool shouldNotify_ = false;
    uint16_t changedMask_ = 0;

    uint32_t consecutiveFailures_ = 0;
    bool inBackoff_ = false;
    size_t backoffStepIndex_ = 0;
};

} // namespace MeasurementService
