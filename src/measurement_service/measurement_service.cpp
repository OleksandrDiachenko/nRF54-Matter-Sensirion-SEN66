/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "measurement_service.h"

#include "measurement_policy.h"
#include "sen66/sen66_driver.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(measurement_service, LOG_LEVEL_INF);

namespace MeasurementService {

namespace {

/* Lowest preemptible priority: this poller must never contend with the CHIP
 * event loop, BLE, or Wi-Fi threads for CPU. Tune empirically if hardware
 * testing shows Matter responsiveness suffering during sensor backoff. */
constexpr int kWorkQueuePriority = CONFIG_NUM_PREEMPT_PRIORITIES - 1;

K_THREAD_STACK_DEFINE(sWorkQueueStack, CONFIG_APP_MEASUREMENT_SERVICE_STACK_SIZE);
K_MUTEX_DEFINE(sSnapshotMutex);

struct k_work_q sWorkQueue;
struct k_work_delayable sPollWork;

Policy sPolicy;
UpdateCallback sUpdateCallback = nullptr;

/* Mutex-guarded copy handed out by GetLatest(); updated only from the
 * work-queue thread inside PollWorkHandler(). */
Snapshot sSnapshot;

void PublishSnapshot(int64_t nowMs) {
    Sen66::Measurement latest{};
    int64_t ageMs = 0;
    const bool everValid = sPolicy.GetLatest(latest, ageMs, nowMs);

    k_mutex_lock(&sSnapshotMutex, K_FOREVER);
    sSnapshot.measurement = latest;
    sSnapshot.ageMs = ageMs;
    sSnapshot.everValid = everValid;
    k_mutex_unlock(&sSnapshotMutex);
}

void PollWorkHandler(struct k_work *work) {
    ARG_UNUSED(work);

    int err = 0;
    if (sPolicy.NeedsRestart()) {
        err = Sen66::StartMeasurement();
        if (err) {
            LOG_WRN("SEN66 restart failed during recovery: %d", err);
        }
    }

    Sen66::Measurement measurement{};
    if (err == 0) {
        err = Sen66::ReadMeasurement(measurement);
    }

    const int64_t now = k_uptime_get();
    sPolicy.OnReadResult(err, measurement, now);

    // Always republish, even on a failed tick: GetLatest()'s age must reflect
    // real elapsed time so a caller can tell a snapshot is going stale during
    // backoff, not just whether the last read happened to succeed.
    PublishSnapshot(now);

    if (sPolicy.ShouldNotify()) {
        Sen66::Measurement latest{};
        int64_t ageMs = 0;
        const uint16_t changedMask = sPolicy.ChangedMask();
        if (sPolicy.GetLatest(latest, ageMs, now)) {
            sPolicy.MarkNotified(now);
            if (sUpdateCallback != nullptr) {
                sUpdateCallback(latest, changedMask);
            }
        }
    }

    k_work_reschedule_for_queue(&sWorkQueue, &sPollWork, K_MSEC(sPolicy.NextDelayMs()));
}

} // namespace

bool Init() {
    k_work_queue_init(&sWorkQueue);
    struct k_work_queue_config cfg = {};
    cfg.name = "sen66_poll";
    k_work_queue_start(&sWorkQueue, sWorkQueueStack, K_THREAD_STACK_SIZEOF(sWorkQueueStack),
                        kWorkQueuePriority, &cfg);

    k_work_init_delayable(&sPollWork, PollWorkHandler);

    const int err = Sen66::StartMeasurement();
    if (err) {
        LOG_WRN("SEN66 initial start-measurement failed: %d (will retry)", err);
        // Feed the failure in so the retry/backoff counter starts from a
        // consistent state instead of silently dropping the first attempt.
        sPolicy.OnReadResult(err, Sen66::Measurement{}, k_uptime_get());
    }

    k_work_schedule_for_queue(&sWorkQueue, &sPollWork, K_NO_WAIT);
    LOG_INF("SEN66 measurement service started");
    return true;
}

bool GetLatest(Snapshot &out) {
    k_mutex_lock(&sSnapshotMutex, K_FOREVER);
    out = sSnapshot;
    k_mutex_unlock(&sSnapshotMutex);
    return out.everValid;
}

void RegisterUpdateCallback(UpdateCallback callback) {
    sUpdateCallback = callback;
}

} // namespace MeasurementService
