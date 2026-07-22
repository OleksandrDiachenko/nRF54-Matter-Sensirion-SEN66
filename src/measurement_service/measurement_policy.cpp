/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "measurement_policy.h"

namespace MeasurementService {

namespace {

uint16_t AbsDeltaU16(uint16_t a, uint16_t b) {
    return a > b ? static_cast<uint16_t>(a - b) : static_cast<uint16_t>(b - a);
}

uint16_t AbsDeltaS16(int16_t a, int16_t b) {
    const int32_t diff = static_cast<int32_t>(a) - static_cast<int32_t>(b);
    return static_cast<uint16_t>(diff < 0 ? -diff : diff);
}

/* Channels that moved by at least their threshold, considering only channels
 * valid in both snapshots (a channel with no prior valid value has nothing to
 * compare against; its validity flip is reported separately by the caller). */
uint16_t DeltaMask(const Sen66::Measurement &prev, const Sen66::Measurement &fresh,
                    const ChangeThresholds &t) {
    const uint16_t bothValid = static_cast<uint16_t>(prev.valid & fresh.valid);
    uint16_t mask = 0;

    if ((bothValid & Sen66::kFieldPm1p0) && AbsDeltaU16(prev.pm1p0, fresh.pm1p0) >= t.pm) {
        mask |= Sen66::kFieldPm1p0;
    }
    if ((bothValid & Sen66::kFieldPm2p5) && AbsDeltaU16(prev.pm2p5, fresh.pm2p5) >= t.pm) {
        mask |= Sen66::kFieldPm2p5;
    }
    if ((bothValid & Sen66::kFieldPm4p0) && AbsDeltaU16(prev.pm4p0, fresh.pm4p0) >= t.pm) {
        mask |= Sen66::kFieldPm4p0;
    }
    if ((bothValid & Sen66::kFieldPm10) && AbsDeltaU16(prev.pm10, fresh.pm10) >= t.pm) {
        mask |= Sen66::kFieldPm10;
    }
    if ((bothValid & Sen66::kFieldHumidity) &&
        AbsDeltaS16(prev.humidity, fresh.humidity) >= t.humidity) {
        mask |= Sen66::kFieldHumidity;
    }
    if ((bothValid & Sen66::kFieldTemperature) &&
        AbsDeltaS16(prev.temperature, fresh.temperature) >= t.temperature) {
        mask |= Sen66::kFieldTemperature;
    }
    if ((bothValid & Sen66::kFieldVocIndex) &&
        AbsDeltaS16(prev.voc_index, fresh.voc_index) >= t.index) {
        mask |= Sen66::kFieldVocIndex;
    }
    if ((bothValid & Sen66::kFieldNoxIndex) &&
        AbsDeltaS16(prev.nox_index, fresh.nox_index) >= t.index) {
        mask |= Sen66::kFieldNoxIndex;
    }
    if ((bothValid & Sen66::kFieldCo2) && AbsDeltaU16(prev.co2, fresh.co2) >= t.co2) {
        mask |= Sen66::kFieldCo2;
    }
    return mask;
}

} // namespace

Policy::Policy(const PolicyConfig &config) : config_(config) {}

void Policy::OnReadResult(int err, const Sen66::Measurement &measurement, int64_t nowMs) {
    shouldNotify_ = false;
    changedMask_ = 0;

    if (err != 0) {
        consecutiveFailures_++;
        if (consecutiveFailures_ >= config_.failureThreshold) {
            constexpr size_t kBackoffStepCount =
                sizeof(config_.backoffStepsMs) / sizeof(config_.backoffStepsMs[0]);
            if (!inBackoff_) {
                inBackoff_ = true;
                backoffStepIndex_ = 0;
            } else if (backoffStepIndex_ + 1 < kBackoffStepCount) {
                backoffStepIndex_++;
            }
        }
        return;
    }

    // A transport-level success proves the sensor is alive and answering,
    // even if every channel is still the warm-up sentinel: recover fully.
    consecutiveFailures_ = 0;
    inBackoff_ = false;
    backoffStepIndex_ = 0;

    latest_ = measurement;
    lastUpdateMs_ = nowMs;
    if (measurement.valid != 0) {
        everValid_ = true;
    }

    EvaluateNotify(measurement);
}

bool Policy::GetLatest(Sen66::Measurement &out, int64_t &ageMs, int64_t nowMs) const {
    if (!everValid_) {
        return false;
    }
    out = latest_;
    ageMs = nowMs - lastUpdateMs_;
    return true;
}

void Policy::MarkNotified(int64_t nowMs) {
    lastNotified_ = latest_;
    lastNotifyMs_ = nowMs;
    shouldNotify_ = false;
    changedMask_ = 0;
}

uint32_t Policy::NextDelayMs() const {
    if (!inBackoff_) {
        return config_.pollIntervalMs;
    }
    return config_.backoffStepsMs[backoffStepIndex_];
}

void Policy::EvaluateNotify(const Sen66::Measurement &fresh) {
    if (fresh.valid == 0 && lastNotified_.valid == 0) {
        // Nothing has ever been valid; there is nothing to report yet.
        return;
    }

    uint16_t mask = static_cast<uint16_t>(fresh.valid ^ lastNotified_.valid);
    mask = static_cast<uint16_t>(mask | DeltaMask(lastNotified_, fresh, config_.thresholds));

    const bool intervalElapsed =
        (lastUpdateMs_ - lastNotifyMs_) >= static_cast<int64_t>(config_.notifyIntervalMs);

    if (mask != 0 || intervalElapsed) {
        shouldNotify_ = true;
        changedMask_ = mask;
    }
}

} // namespace MeasurementService
