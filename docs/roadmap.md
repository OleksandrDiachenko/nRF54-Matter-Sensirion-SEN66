# Roadmap

## M0 — Project bootstrap

Branch: `chore/project-bootstrap`

- Document architecture, wiring, workflow, and milestones.
- Establish `main` as the integration branch.
- Add repository hygiene files.

Done when the repository documents a reproducible direction and this branch is
merged through a pull request.

## M1 — Matter over Wi-Fi baseline

Branch: `feature/matter-wifi-baseline`

- Start from the Nordic Matter template.
- Build for `nrf54lm20dk/nrf54lm20a/cpuapp` with `nrf7002eb2`.
- Commission a sensor-free accessory to `BUCKET` using QR or manual onboarding.
- Confirm Wi-Fi commissioning and reconnect-after-reset with `chip-tool`.

Apple Home cannot add the accessory yet: the ZAP file exposes only the Root
Node endpoint until M4 defines the Air Quality Sensor endpoint, so there is
nothing for Apple Home to display. Full Apple Home acceptance is M5.

## M2 — SEN66 driver

Branch: `feature/sen66-driver`

- Add DeviceTree wiring and an isolated I2C driver.
- Validate command responses and Sensirion CRC per word.
- Parse serial number and measured values with correct signedness and
  unavailable-value handling.
- Add parser and CRC unit tests plus a hardware smoke test.

## M3 — Measurement service

Branch: `feature/measurement-service`

- Poll continuous measurements without blocking the Matter event loop.
- Handle warm-up, I2C errors, CRC errors, retry, and recovery.
- Expose a typed latest measurement with explicit validity.

## M4 — Matter air-quality endpoint

Branch: `feature/matter-air-quality`

- Define the Air Quality Sensor endpoint in ZAP.
- Publish temperature, humidity, CO2, PM1, PM2.5, and PM10 correctly.
- Define and test the overall-air-quality policy.
- Verify all supported attributes with `chip-tool`.

## M5 — Apple Home acceptance

Branch: `feature/apple-home-integration`

- Commission through Apple Home.
- Verify visible tiles and updates.
- Test reconnect, sensor removal, and hotspot restart.

## M6 — NFC onboarding

Branch: `feature/nfc-onboarding`

- Enable `CONFIG_CHIP_NFC_ONBOARDING_PAYLOAD`.
- Validate commissioning from an iPhone NFC tap.
- Preserve QR/manual onboarding as fallback.

## M7 — Hardening and releases

- Add CI build and unit-test checks.
- Add diagnostics, factory-reset verification, and DFU planning.
- Tag milestones `v0.1.0`, `v0.2.0`, and `v1.0.0`.
