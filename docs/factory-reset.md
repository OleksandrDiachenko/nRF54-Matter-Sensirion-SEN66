# Factory reset verification (M7)

Verifies the factory-reset behavior already noted in passing during M5
([apple-home.md](apple-home.md#re-opening-commissioning-without-losing-wi-fi))
with a dedicated hardware pass and explicit acceptance criteria.

## Trigger

Hold Button 1 for ~6 s (3 s arm + 3 s cancel window). The serial log prints:

```text
Factory reset has been triggered. Release button within 3000ms to cancel.
```

Releasing before the cancel window elapses aborts the reset; holding
through it commits and reboots the board.

## Acceptance criteria and results

| Check | Result | Evidence |
| --- | --- | --- |
| Fabric removed | Pass | Device's operational mDNS record (`_matter._tcp`) disappeared after reset; `matter fabrics`-equivalent state confirmed indirectly via mDNS browse before/after |
| Settings erased | Pass | `Last Known Good Time: [unknown]` on the post-reset boot, vs. a real timestamp normally restored from settings on a plain reboot |
| Wi-Fi credentials erased | Pass | 30+ s of post-reset uptime with zero reconnect attempts to the previously-joined network (a plain reboot instead retries roughly every ~9 s per [apple-home.md](apple-home.md)) |
| Onboarding payload preserved | Pass | Same PIN (`20202021`) and discriminator (`0xF00`/3840) reprinted from factory data; QR/manual codes regenerated but decode to the same values |
| BLE commissioning window reopens | Pass | `CHIPoBLE advertising started` immediately after the post-reset boot |
| NFC onboarding reopens | Pass | `NFC Tag emulation started` immediately after the post-reset boot |
| Re-commissioning succeeds | Pass | Re-added via Apple Home after the reset; fabric persisted (`Metadata for Fabric 0x1 persisted to storage`), CASE session established, device reachable over IP/mDNS from a second machine on the same network afterward |

## Known limitation: commissioner-side Wi-Fi flap

Not a factory-reset defect, but discovered during this pass: if the
*commissioner's* own Wi-Fi interface disconnects and reconnects (e.g. the
phone switches networks, or the Mac running `chip-tool` is on a different
network than the accessory), the existing Home app subscription does not
reliably resume automatically - tiles stop updating with no visible error.

Confirmed recovery paths, either of which is sufficient:

- Retrying the reconnect a second time.
- Closing the Home app for an extended period, then reopening it.

This matches a subscription-resumption retry loop visible in the device's
own serial log (`Failed to establish CASE for subscription-resumption with
error '32'`, repeating on a growing backoff) - the device keeps trying on
its side, but the Home app's own session recovery is what's unreliable.
Root cause is on the controller/Home app side, not this firmware; no
action taken here beyond documenting it.

## Commissioner network prerequisite

For any external tool (`chip-tool`, etc.) to complete BLE-WiFi
commissioning or read attributes after Wi-Fi handoff, the commissioner
itself must be on the *same* IP network as the accessory - CASE
establishment and mDNS discovery both happen over IP, not BLE, once the
Wi-Fi credentials are handed off. A `chip-tool` commissioning attempt from
a machine on a different network gets through BLE pairing, attestation,
and `AddNOC`, but then times out on `WiFiNetworkEnable`/CASE and the
device automatically rolls back the uncommitted fabric once the
commissioning fail-safe timer (60-120 s, extended by the commissioner's
retries) expires - confirmed on hardware, no manual cleanup needed.
