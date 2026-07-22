/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "sen66/sen66_protocol.h"

#include <stdint.h>

namespace MeasurementService {

/* Thread-safe copy of the measurement policy's latest state. */
struct Snapshot {
    Sen66::Measurement measurement{}; // raw fixed-point + per-channel valid bitmask
    int64_t ageMs = 0;                // time since this snapshot was read
    bool everValid = false;           // false until the first valid frame ever arrives
};

/*
 * Start the dedicated work queue that polls the SEN66 roughly once a second,
 * retries and backs off on I2C/CRC failure, and recovers automatically once
 * the sensor answers again. Requires Sen66::Init() to have already returned
 * true. Safe to call once from the app/main thread at startup.
 */
bool Init();

/*
 * Copy out the latest snapshot. Never touches I2C and never blocks on the
 * sensor, so it is safe to call from any thread (shell, CHIP, app). Returns
 * false only when no valid frame has ever been observed (still booting).
 */
bool GetLatest(Snapshot &out);

/*
 * Invoked from the measurement service's own work-queue thread whenever the
 * notify policy decides an update is worth publishing (roughly every 5-10s,
 * or immediately on a significant per-channel change or validity flip).
 * `changedMask` uses the Sen66::Field bits; 0 means a plain interval
 * heartbeat with no channel-level change. The callback runs in the
 * measurement service's thread context, not the CHIP thread: a future
 * Matter adapter must marshal its own attribute writes onto the CHIP thread
 * (e.g. via PlatformMgr().ScheduleWork()) rather than call into Matter here.
 */
using UpdateCallback = void (*)(const Sen66::Measurement &snapshot, uint16_t changedMask);
void RegisterUpdateCallback(UpdateCallback callback);

} // namespace MeasurementService
