# Measurement service

The measurement service (milestone M3) is the layer between the SEN66 driver
and the Matter endpoint (M4, see [air-quality-policy.md](air-quality-policy.md)
and `src/air_quality_endpoint/`). It polls the sensor, survives I2C/CRC errors
and a disconnected sensor, and exposes a typed, thread-safe latest
measurement. It does not touch Matter itself - see "Notify policy" below for
how the Matter adapter consumes it instead.

## Modules

Sources live in `src/measurement_service/` under namespace
`MeasurementService`, split the same way as the driver
([sen66-driver.md](sen66-driver.md)) so the decision logic is host-testable:

| File | Role | Zephyr dependency |
| --- | --- | --- |
| `measurement_policy.{h,cpp}` | poll/retry/backoff/notify decisions | none (host-testable) |
| `measurement_service.{h,cpp}` | work queue, mutex, driver calls | `zephyr/kernel.h` |

`CONFIG_APP_MEASUREMENT_SERVICE` gates the service and depends on
`CONFIG_APP_SEN66`.

## Threading

A dedicated `k_work_q`, with its own stack
(`CONFIG_APP_MEASUREMENT_SERVICE_STACK_SIZE`) and a low preemptible priority,
runs a `k_work_delayable` poll handler roughly once a second. All SEN66
driver calls - and their internal `k_msleep()` - happen only on this thread.
Neither the CHIP/Matter event-loop thread nor the app's main dispatch thread
(`Nrf::DispatchNextTask()`) ever calls into the driver directly, so a slow or
stalled sensor cannot block Matter.

Each tick calls `Sen66::ReadMeasurement()` directly - not
`Sen66::IsDataReady()` first. `IsDataReady()` collapses every failure mode
(I2C error, CRC error, "not ready yet") into `false`, so it cannot tell a
transport fault from ordinary warm-up. `ReadMeasurement()` gives an explicit
error code for transport/CRC failures, while "still warming up" already shows
up as `valid == 0` on an otherwise successful read, because the sensor
returns a sentinel-filled frame until its first internal measurement
completes. This halves the I2C traffic per tick and needs no extra warm-up
state machine.

## Retry, backoff, and recovery

The policy counts consecutive `ReadMeasurement()` failures:

- Below the failure threshold (default 3): log and keep polling at the
  normal ~1 s cadence.
- At or above the threshold: reissue `Sen66::StartMeasurement()` before each
  further attempt and back off through an escalating delay (2 s, 5 s, 10 s,
  capped at 30 s), so a genuinely absent sensor is not hammered.
- The first successful read after backoff - even one where every channel is
  still the warm-up sentinel - resets the failure counter and cadence
  immediately. A successful, CRC-valid response already proves the sensor is
  back, whether it dropped off the bus or just power-cycled.

This is the same mechanism whether the fault was a transient I2C glitch or a
physically disconnected/reconnected sensor; the service does not distinguish
them beyond how long the failures persist.

## Latest-measurement snapshot

`MeasurementService::GetLatest()` copies out a `Snapshot` (the raw
`Sen66::Measurement`, its age in milliseconds, and whether any valid frame
has ever been observed) under a mutex. It never touches I2C and never
blocks, so it is safe to call from any thread - the shell, and later the
Matter adapter. As with the driver, a cleared `valid` bit must never be read
as a value.

## Notify policy

The service also decides when an update is worth publishing, so the Matter
adapter does not have to duplicate that logic: an `UpdateCallback` fires from
the work-queue thread when either a channel changes by more than its
configured threshold, a channel's validity flips, or roughly 5-10 s (default
7 s) have passed since the last notification (a heartbeat with
`changedMask == 0`). The callback runs on the measurement service's own
thread; `src/air_quality_endpoint/air_quality_matter_adapter.cpp` (M4) is the
registered consumer, and it marshals every attribute write onto the CHIP
thread via `PlatformMgr().ScheduleWork()` rather than calling into Matter
from this callback directly, per the architecture boundary in
[architecture.md](architecture.md).

## Testing

- **Unit tests** (`tests/measurement_service/`, host `type: unit`) cover
  steady-state polling, warm-up (no failure counted), transient-failure
  recovery, sustained-failure backoff and its escalation/cap, recovery after
  backoff, validity-transition notifications, and the change-threshold vs.
  heartbeat notify policy. Run with
  `west twister -T tests/measurement_service -p unit_testing`.
- **Hardware smoke test**: the `sen66 latest` shell command prints the
  service's current snapshot (age and per-channel validity) without issuing
  a blocking read of its own, so it can observe polling, backoff, and
  recovery live - including after physically disconnecting and reconnecting
  the sensor - without needing Matter. See [building.md](building.md).
