# Continuous integration

`.github/workflows/ci.yml` runs on every push to `main` and every pull
request. Two jobs run in parallel, both inside the official
`ghcr.io/nrfconnect/sdk-nrf-toolchain:v3.4.0` container so the toolchain
matches [the local build environment](building.md) exactly:

- **Unit tests** — `west twister -T tests -p unit_testing`, the same command
  documented in [building.md](building.md#sen66-driver-unit-tests) for the
  `sen66`, `measurement_service`, and `air_quality_endpoint` suites. Unlike
  local development on macOS, the Linux container compiles the real Zephyr
  `ztest` binaries (see the macOS limitation noted in building.md).
- **Firmware build** — the full Matter-over-Wi-Fi sysbuild image for
  `nrf54lm20dk/nrf54lm20a/cpuapp` with `nrf7002eb2`, the exact command from
  [building.md](building.md#build-command). This only proves the image
  links; it does not flash or run on hardware.

This repository is a freestanding NCS application (no `west.yml` of its
own), so both jobs first provision an NCS v3.4.0 west workspace with
`west init -m https://github.com/nrfconnect/sdk-nrf --mr v3.4.0` and
`west update --narrow -o=--depth=1`, then build this repository as an
out-of-tree app against it — mirroring how the toolchain is set up locally
(see the CLI build notes referenced from building.md). That workspace
(zephyr, nrf, and modules, including Matter) is a multi-gigabyte checkout,
so it is cached by NCS version between runs; only the first run after a
cache eviction pays the full `west update` cost.

Both jobs are required checks for the acceptance criteria in
[development-workflow.md](development-workflow.md).
