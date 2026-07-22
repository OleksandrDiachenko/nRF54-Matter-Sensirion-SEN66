# Apple Home acceptance (M5)

M5 accepts the endpoint defined in M4 into Apple Home. This documents how to
make the accessory commissionable again without losing its Wi-Fi
provisioning, and records what the current iOS Home app actually renders and
how it behaves across the resilience scenarios from
[roadmap.md](roadmap.md).

## Re-opening commissioning without losing Wi-Fi

`Board::StartBLEAdvertisement()` (NCS common Matter sample library) refuses
to open the BLE commissioning window while the device already has a fabric -
it logs `"Matter service BLE advertising not started - device is already
commissioned"` and returns, both on the boot-time pairing autostart and on a
Button 1 press. So a device left over from a previous commissioning (e.g. the
`chip-tool` -> `BUCKET` pairing from M1) will not respond to a new QR/manual
onboarding attempt at all - this looks like an unresponsive device on the
commissioner side, not an error.

Removing the fabric makes it commissionable again, but the NCS default for
"what happens when the last fabric is removed" is a full factory reset,
including Wi-Fi station credentials
(`CONFIG_CHIP_LAST_FABRIC_REMOVED_ERASE_AND_REBOOT`). This project instead
sets `CONFIG_CHIP_LAST_FABRIC_REMOVED_NONE=y` in `prj.conf`, so removing the
fabric touches nothing else. To make an already-commissioned device
available to a new commissioner (chip-tool, Apple Home, ...) without
retyping the Wi-Fi password:

```console
# From the Mac, against the existing fabric/node:
/opt/nordic/ncs/v3.4.0/modules/lib/matter/out/chip-tool/chip-tool pairing unpair <node-id>
```

**Reboot the board** (a plain reset, not a factory-reset hold) to reopen the
BLE commissioning window. `chip::Server::Init()` opens the basic
commissioning window automatically at boot whenever `FabricCount() == 0`
(`CONFIG_CHIP_ENABLE_PAIRING_AUTOSTART=y`) - this check is independent of
Wi-Fi/network state. The device stays joined to `BUCKET` throughout; the
discriminator and setup passcode are fixed in factory data, so the
QR/manual code printed at boot and the NFC tag payload remain valid across
re-commissioning.

**Button 1 does *not* do this once Wi-Fi stays connected**, which is easy to
assume from the NCS common board library's naming but is wrong for this
project's configuration: `Board::StartBLEAdvertisementHandler()` branches on
its own `mState == DeviceProvisioned`, which tracks IPv6/Wi-Fi network
provisioning, not Matter fabric/commissioning state. Since
`CONFIG_CHIP_LAST_FABRIC_REMOVED_NONE=y` deliberately keeps Wi-Fi connected
after an unpair, `mState` stays `DeviceProvisioned` forever, so a Button 1
press always takes the "software update" branch instead
(`CONFIG_CHIP_DFU_OVER_BT_SMP` is off in this project, so it just logs
`"Software update is disabled"` and does nothing) - confirmed on hardware:
pressing Button 1 after an unpair produced exactly that log line, not a
reopened commissioning window. Only a reboot reliably reopens it here.

Button 1 reference (NCS common board library), as actually observed in this
project:

| Action | Effect |
| --- | --- |
| Short press | Restart BLE **SMP/DFU** advertising if `CONFIG_CHIP_DFU_OVER_BT_SMP` is enabled - a no-op here (`"Software update is disabled"`) whenever Wi-Fi is already connected, regardless of fabric state |
| Hold ~6 s (3 s arm + 3 s cancel window) | Full factory reset: erases the fabric, Wi-Fi credentials, and all settings, then reboots |

See [factory-reset.md](factory-reset.md) for the dedicated M7 verification
pass with explicit acceptance criteria and results.

## Commissioning

Attempt all three onboarding paths available from this firmware (they share
the same discriminator/passcode, so this is cheap to cover):

- QR code from the serial log (`vcom0`, 115200 8N1).
- Manual pairing code, same log.
- NFC tap (`CONFIG_CHIP_NFC_ONBOARDING_PAYLOAD=y` is already enabled ahead of
  M6).

Before scanning, confirm on the iPhone (joined to `BUCKET`):

- Safari loads a real page - the Home app's Matter setup flow needs internet
  reachability, not just local link connectivity to the accessory.
- A Home already exists in the Home app under the signed-in Apple ID.
- The `BUCKET` password is available to type manually if the Home app
  prompts for it instead of auto-filling it.

Record the exact outcome of each attempt below (success, or the exact
on-screen error/message on failure):

- QR: succeeded (used for the clean single-fabric pairing this document's
  other results are based on).
- Manual code: not separately tested - it carries the same
  discriminator/passcode as the QR payload, so it is the same commissioning
  path with a different transcription step, not re-verified independently.
- NFC: succeeded once from a fully factory-reset state, but failed on a
  later retry attempt (tapped, nothing happened) while QR succeeded
  immediately after in the same session. Inconsistent - worth a closer look
  if NFC onboarding becomes load-bearing in M6, but out of scope here since
  QR/manual remain the primary, reliable onboarding path for M5.

## Clusters rendered as native Home tiles

Endpoint 1 publishes the clusters listed in
[architecture.md](architecture.md). The Home app only builds a tile for
clusters/device types it has native UI support for; the rest may still be
readable via `chip-tool` even if iOS shows nothing for them. Observed after
a successful commissioning below; for future re-checks, cross-check each
visible value against `sen66 latest` or a
`chip-tool <cluster> read measured-value` (~7 s notify cadence, see
[measurement-service.md](measurement-service.md)):

| Matter cluster | Native Home tile? | Observed value |
| --- | --- | --- |
| Temperature Measurement (endpoint 1) | No - absent from the accessory detail view entirely | n/a |
| Relative Humidity Measurement | Yes - "Current Relative Humidity" | 59% (not cross-checked against `sen66 latest` in this pass) |
| Carbon Dioxide Concentration Measurement | Yes - "Carbon dioxide Level" | 1,621 ppm (not cross-checked against `sen66 latest` in this pass) |
| PM1 Concentration Measurement | No - absent from the accessory detail view entirely | n/a |
| PM2.5 Concentration Measurement | Yes - "PM2.5 Density" | 13 ug/m3 (not cross-checked against `sen66 latest` in this pass) |
| PM10 Concentration Measurement | Yes - "PM10 Density" | 17 ug/m3 (not cross-checked against `sen66 latest` in this pass) |
| Air Quality | Yes - accessory subtitle and its own row ("Poor") | n/a (categorical) |
| Temperature Measurement (endpoint 2, dedicated Temperature Sensor device type) | Yes - confirmed on hardware after re-commissioning | n/a (not cross-checked against `sen66 latest` in this pass) |

Observed on the accessory detail screen ("Living Room Matter Accessory"):
Air Quality, PM2.5 Density, PM10 Density, Carbon dioxide Level, Current
Relative Humidity. Temperature Measurement (endpoint 1) and PM1
Concentration Measurement are published by the endpoint (see
[architecture.md](architecture.md)) but the Home app does not surface a
tile or detail row for either - not a bug in this firmware, just what the
current Home app's Air Quality Sensor device type UI renders.

iOS/Home app version not recorded for this pass.

**Endpoint 2 (standalone Temperature Sensor device type)** was added after
this pass specifically to work around the Temperature Measurement gap above
- see [architecture.md](architecture.md). Confirmed on hardware: after
re-commissioning, Home renders a native temperature tile for it, unlike
endpoint 1's copy of the same cluster.

VOC/NOx were also tried as `LevelIndication`-only Concentration Measurement
clusters (see git history) but reverted: Home rendered no tile for either
even after endpoint 2 above proved Home had picked up the new firmware's
data model, so the most likely explanation is Home's bridge has no mapping
for a Concentration Measurement cluster that exposes only `LevelValue` (no
`MeasuredValue`). Not re-attempted since - SEN66's VOC/NOx indexes stay
off Matter entirely, readable only via `sen66 latest`/`sen66 read`.

## Resilience

For each scenario: reproduce it, then record pass/fail and any observed
delay or manual step below.

| Scenario | Result | Notes |
| --- | --- | --- |
| Reboot the board | Pass | After a clean single-fabric pairing, reconnect was fast: steady `ReportData`/subscription traffic resumed with no mDNS resolution timeouts and no re-pairing needed. (An earlier attempt with multiple stray fabrics from repeated re-commissioning attempts showed ~45 s mDNS timeouts per stale fabric before recovering - resolved by a factory reset back to a single fabric.) |
| Restart the Android hotspot | Pass | Board retried the Wi-Fi connection roughly every ~9 s while the hotspot was down (`"Connection to WiFi network failed or was terminated by another request"`), reconnected automatically once it came back, re-established the same CASE session/fabric (no re-pairing), and resumed `ReportData` within about a second of the network returning. No manual action needed. |
| Temporary Wi-Fi loss | Covered by the hotspot-restart scenario above | Same underlying failure mode (Wi-Fi drop -> `WiFiManager` reconnect retry -> CASE resumption) and same result; not re-tested separately |
| Disconnect the SEN66 | Pass (firmware) / Home UI shows stale values | `measurement_service` correctly detects the fault and retries with backoff (`"SEN66 restart failed during recovery: -5"` at growing 5s/10s/20s intervals, matching the M3-documented policy); the underlying Matter attributes go null per the M4 `chip-tool` verification. However, the Home app tile does **not** show "No Data" - it just keeps displaying the last value it received before the disconnect, with no visible staleness indicator. This is a Home app UI limitation (it has no "unavailable" affordance for these sensor types), not a firmware defect. On reconnecting the sensor, readings recovered on their own (no reboot) and the Home app picked up fresh values, matching the M3-documented recovery behavior. |

## Known limitations without a Home Hub

This setup has no HomePod, Apple TV, or hub-capable iPad - only an Android
phone hotspot, an iPhone, the accessory, and this Mac. Given that:

- Control is local-only, and only while the iPhone is joined to `BUCKET`.
- There is no remote access to the accessory away from `BUCKET`.
- Rich Home automations that depend on hub-mediated triggers are not
  available.

This is expected scope for M5, not a defect - full remote/automation support
is out of scope until a Home Hub is introduced.
