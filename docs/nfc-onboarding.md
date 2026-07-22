# NFC onboarding (M6)

M6 validates commissioning from an iPhone NFC tap, on top of the QR/manual
onboarding already confirmed in M1 and M5. `CONFIG_CHIP_NFC_ONBOARDING_PAYLOAD=y`
was already enabled in `prj.conf` ahead of this milestone (see
[apple-home.md](apple-home.md)); this document is the validation record.

## How NFC onboarding actually works here

NFC tag emulation is not an independent onboarding channel - it is entirely
driven by the BLE commissioning window's state. In the NCS common Matter
sample library
(`/opt/nordic/ncs/v3.4.0/nrf/samples/matter/common/src/app/matter_event_handler.cpp`),
`NFCOnboardingPayloadMgr().StartTagEmulation()` /
`StopTagEmulation()` are called directly from the `kCHIPoBLEAdvertisingChange`
event handler:

```cpp
case DeviceEventType::kCHIPoBLEAdvertisingChange:
    if (event->CHIPoBLEAdvertisingChange.Result == kActivity_Started)
        NFCOnboardingPayloadMgr().StartTagEmulation(...);
    else if (event->CHIPoBLEAdvertisingChange.Result == kActivity_Stopped)
        NFCOnboardingPayloadMgr().StopTagEmulation();
```

So the NFC tag is only "live" while the BLE commissioning window is open -
the same `FabricCount() == 0` condition documented in
[apple-home.md](apple-home.md) for QR/manual. Once a fabric exists (or a BLE
connection is in progress), the tag stops responding. This is expected, not
a defect: an already-commissioned device correctly does not invite a new
NFC tap to onboard it.

## What was actually wrong before (M5 record)

M5's acceptance record noted one successful NFC tap and one that appeared to
fail ("tapped, nothing happened"), left as an open question. Retested for
M6 with a clean single-fabric state and close attention to what the phone
actually did on each tap:

**Every tap reliably triggered iOS's system NFC notification banner.** The
earlier "failure" was the banner being dismissed/cancelled instead of
tapped through to open the Add Accessory flow - not a firmware or NFC
hardware issue. `NFCOnboardingPayloadManagerImpl.cpp` also logs a
`ChipLogError` on every failure path in `_StartTagEmulation` (URI encode,
payload set, `nfc_t2t_emulation_start`), and none of those errors appeared
in any capture, which independently confirms the tag emulation stack itself
never failed.

**Confirmed on hardware, this session:** two full, independent NFC-tap
commissioning cycles, each starting from a clean factory-reset
(`FabricCount() == 0`), each completing the entire flow (PASE, ArmFailSafe,
certificate chain, CSR, `AddNOC`, `CommissioningComplete`,
`Fail-safe cleanly disarmed`) without error. Both times, Apple Home then
added a second admin fabric of its own via an existing CASE session (visible
as a second `AddTrustedRootCertificate`/`AddNOC` sequence over UDP,
immediately following the NFC-triggered one) - this is normal Apple Home
behavior already documented in [apple-home.md](apple-home.md), not something
NFC-specific.

## Correct NFC tap procedure

1. Device must have `FabricCount() == 0` (fresh factory reset, or after
   fully removing all fabrics - see the fabric-accumulation note below).
2. Bring the iPhone close to the DK's NFC antenna and hold briefly.
3. iOS shows a system notification banner for the discovered NFC tag -
   **tap the banner** to proceed into Add Accessory. Dismissing/cancelling
   it (swiping away, ignoring it) is indistinguishable from "nothing
   happened" but is a user action, not a device fault.

QR and manual code remain the reliable fallback and were not affected by
any of this - they use the same discriminator/passcode and the same
underlying BLE commissioning window.

## Fabric accumulation across repeated test cycles

Discovered while testing: removing the accessory from the Home app (or via
`chip-tool pairing unpair`) only removes the fabric belonging to that
specific admin/controller - `RemoveFabric` is a self-removal operation by
design in Matter, a controller cannot remove another admin's fabric. Across
repeated M5/M6 commissioning cycles, the device accumulated more than one
stray fabric (visible as multiple distinct compressed-fabric-ID prefixes in
the mDNS operational advertisements over the course of testing). When that
happens, `FabricCount() != 0` persists even after "removing" the accessory
from the app you're currently using, and the BLE/NFC commissioning window
will not reopen on reboot - it looks identical to the original M1/M5
"already commissioned" symptom.

There is no supported way to remove a specific stray fabric you don't have
admin keys for (checked: no fabric-management shell command exists in this
build's `CONFIG_CHIP_LIB_SHELL` command set). A full factory reset (hold
Button 1 ~6 s) is the only reliable way back to a clean, single-fabric
state after heavy experimental re-commissioning. This isn't a bug to fix -
it's a direct consequence of Matter's per-admin fabric-removal security
model - but it's worth knowing before assuming "Remove Accessory" alone
guarantees a clean slate for the next test.

## Definition of done

- NFC tag emulation confirmed reliable: banner appeared on every tap
  attempted, and two independent full commissioning cycles completed
  without any `ChipLogError` in the NFC/tag-emulation path.
- Root cause of the earlier apparent failure identified (UX: banner
  dismissed, not tapped) and documented, closing the open question left by
  M5.
- QR/manual onboarding confirmed unaffected, remaining the fallback per the
  roadmap.
- Fabric-accumulation behavior across repeated test cycles documented,
  with the correct recovery procedure (factory reset).
