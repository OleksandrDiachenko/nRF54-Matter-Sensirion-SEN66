# Building the Matter-over-Wi-Fi baseline

## Prerequisites

- nRF Connect SDK 3.4.0 LTS
- nRF54LM20 DK with the `nrf7002eb2` shield attached
- The nRF Connect SDK environment loaded in the terminal

## Build command

From the repository root, build the nRF54LM20A target with sysbuild:

```console
west build -p -b nrf54lm20dk/nrf54lm20a/cpuapp --sysbuild -- \
  -DSHIELD=nrf7002eb2 \
  -DSB_CONFIG_WIFI_NRF70=y \
  -DCONFIG_CHIP_WIFI=y
```

`SHIELD=nrf7002eb2` selects the nRF7002-EB II hardware overlay.
`SB_CONFIG_WIFI_NRF70=y` enables the nRF70 Wi-Fi image in sysbuild, and
`CONFIG_CHIP_WIFI=y` selects Matter over Wi-Fi rather than Thread.

## Flashing

Use the **Flash** action in nRF Connect for VS Code for the configured
sysbuild build directory. It programs the complete image set.

## Baseline acceptance criteria

1. The image builds without configuration or linker errors.
2. On first boot the serial log prints a Matter onboarding payload.
3. The device is commissionable with QR/manual onboarding to the `BUCKET`
   hotspot.
4. The device reconnects after reboot.
5. NFC exposes the same Matter onboarding payload. It does not include Wi-Fi
   credentials; the commissioner sends those during Matter commissioning.

## UART logs during bring-up

The `nrf7002eb2` shield routes the Zephyr console and shell to `uart30`. On the
connected nRF54LM20-DK, open **vcom: 0** in nRF Connect Serial Terminal
(typically `/dev/tty.usbmodem0010518039041` on this Mac) at **115200 baud, 8N1,
no flow control**.

After a valid MCUboot image starts, the baseline prints these application
markers:

- `SEN66 Matter baseline starting`
- `Preparing Matter server`
- `Matter server prepared`
- `Starting Matter server`

With the SEN66 driver present it also prints `SEN66 I2C ready ...` once the bus
is verified (or a warning if the sensor is absent — that never blocks Matter).

## SEN66 driver unit tests

The Sensirion CRC and the measurement/serial parser are pure and run on the
host with Twister:

```console
west twister -T tests/sen66 -p unit_testing
```

The pure sources use only `<stdint.h>`/`<stddef.h>`, so the parser/CRC compile
into the host `type: unit` binary. (Zephyr `ztest` builds on Linux/CI hosts; on
macOS the ztest section attributes are not accepted by Apple clang.)

## Measurement service unit tests

The poll/retry/backoff/notify policy (`src/measurement_service/`) is pure and
runs the same way:

```console
west twister -T tests/measurement_service -p unit_testing
```

See [measurement-service.md](measurement-service.md) for what it covers.

## Air-quality policy unit tests

The overall Air Quality classification (`src/air_quality_endpoint/`) is pure
and runs the same way:

```console
west twister -T tests/air_quality_endpoint -p unit_testing
```

See [air-quality-policy.md](air-quality-policy.md) for the cited thresholds
it verifies.

## SEN66 hardware smoke test

With the firmware flashed and the sensor wired, use the Matter shell (vcom0) to
exercise the real sensor before any Matter integration:

```console
uart:~$ sen66 serial      # prints the sensor serial number
uart:~$ sen66 read        # prints one measurement; unavailable channels show <n/a>
uart:~$ sen66 latest      # prints the measurement service's latest snapshot (non-blocking)
```

`sen66 latest` reads the measurement service's snapshot instead of issuing a
fresh I2C transaction, so it also reflects in-progress backoff or recovery.
To exercise M3's recovery path on hardware: run `sen66 latest` a few times a
second while physically disconnecting the sensor's I2C lines or power, watch
the age climb and warning logs appear, then reconnect and confirm it recovers
on its own (no reboot) within a few backoff cycles.

Expected observations, confirmed on hardware:

- PM4.0 and PM10 commonly equal PM2.5 indoors. The SEN66 derives these as a
  cumulative mass over a particle-size histogram; with no coarse particles
  (> 2.5 um) present, the larger bins add no mass. A steady PM1.0/PM2.5 ratio
  across repeated reads while the overall level drifts (e.g. settling dust)
  confirms this is real sensor physics, not a parser fault.
- The NOx index commonly reads `1` for an extended period after power-up. The
  datasheet's NOx conditioning time is long (up to a few hours); `1` is the
  algorithm's floor value during that window, not an error.

## Regenerating the ZAP endpoint

`src/default_zap/air_quality_sensor.zap` is the single source of truth for
the Matter data model; `air_quality_sensor.matter` and everything under
`zap-generated/` are derived files that must be regenerated and committed
together whenever the `.zap` file changes - the build does **not** regenerate
them (`ncs_configure_data_model()` passes `BYPASS_IDL`, so
`src/default_zap/zap-generated/*` is read as-is at build time).

Regeneration needs the ZAP CLI tool (a separate download from the NCS
toolchain; version pinned by `modules/lib/matter/scripts/setup/zap.json`),
available headless with no Electron GUI required:

```console
# Once: download the pinned zap-cli build referenced by zap.json and
# point ZAP_INSTALL_PATH at it (see scripts/tools/zap/zap_download.py for
# the exact release URL logic).
export ZAP_INSTALL_PATH=/path/to/extracted/zap-cli

NCS_MATTER=/opt/nordic/ncs/v3.4.0/modules/lib/matter

# 1. Regenerate zap-generated/*.h/*.cpp.
python3 "$NCS_MATTER/scripts/tools/zap/generate.py" \
  --no-prettify-output \
  --templates "$NCS_MATTER/src/app/zap-templates/app-templates.json" \
  --output-dir src/default_zap/zap-generated \
  --zcl "$NCS_MATTER/src/app/zap-templates/zcl/zcl.json" \
  --parallel \
  "$(pwd)/src/default_zap/air_quality_sensor.zap"

# 2. Regenerate the human-readable .matter IDL export.
python3 "$NCS_MATTER/scripts/tools/zap/generate.py" \
  --output-dir /tmp/zap-matter-out \
  --zcl "$NCS_MATTER/src/app/zap-templates/zcl/zcl.json" \
  --matter-file-name "$(pwd)/src/default_zap/air_quality_sensor.matter" \
  "$(pwd)/src/default_zap/air_quality_sensor.zap"
```

The `--zcl` flag is required: `generate.py`'s ZCL-path autodetection resolves
the `.zap` file's embedded `pathRelativity: relativeToZap` package entry
assuming a full west workspace layout several directories deeper than this
freestanding app actually lives, so autodetection resolves to a nonexistent
path here. `--matter-file-name` must point at an *existing* file (it is
overwritten in place); it cannot create a new one.

Commit `air_quality_sensor.zap`, the regenerated `air_quality_sensor.matter`,
and every file under `zap-generated/` together as one change.

## Matter air-quality endpoint verification (chip-tool)

With the device commissioned (see [architecture.md](architecture.md) for the
Wi-Fi/NFC commissioning flow) and its SEN66 wired up, verify endpoint 1 with
`chip-tool` against a real reading - cross-check numeric values with a
concurrent `sen66 latest` shell read, allowing for the ~7 s notify cadence
documented in [measurement-service.md](measurement-service.md).

**1. Endpoint sanity:**

```console
chip-tool descriptor read server-list <node-id> 1
```

Confirms the cluster list matches `air_quality_sensor.zap`.

**2. Per-attribute reads:**

```console
chip-tool temperaturemeasurement read measured-value <node-id> 1
chip-tool relativehumiditymeasurement read measured-value <node-id> 1
chip-tool carbondioxideconcentrationmeasurement read measured-value <node-id> 1
chip-tool carbondioxideconcentrationmeasurement read measurement-medium <node-id> 1   # expect Air
chip-tool carbondioxideconcentrationmeasurement read measurement-unit <node-id> 1     # expect Ppm
chip-tool pm1concentrationmeasurement read measured-value <node-id> 1
chip-tool pm25concentrationmeasurement read measured-value <node-id> 1
chip-tool pm10concentrationmeasurement read measured-value <node-id> 1
chip-tool airquality read air-quality <node-id> 1
chip-tool airquality read feature-map <node-id> 1   # expect Fair|Moderate|VeryPoor|ExtremelyPoor set

# Endpoint 2: standalone Temperature Sensor device type (Apple Home tile
# workaround - see apple-home.md). Same reading as endpoint 1's Temperature
# Measurement cluster.
chip-tool descriptor read server-list <node-id> 2
chip-tool temperaturemeasurement read measured-value <node-id> 2
```

**3. Null-on-invalid check:** physically disconnect the SEN66 (as in the
hardware smoke test above), wait for the age to climb, then re-read each
`measured-value`. Every disconnected channel must read back Matter-null, not
`0` or a frozen stale value.

**4. Reporting/subscription check** (a passing read does not prove reporting
works):

```console
chip-tool pm25concentrationmeasurement subscribe measured-value 1 10 <node-id> 1
chip-tool airquality subscribe air-quality 1 10 <node-id> 1
chip-tool temperaturemeasurement subscribe measured-value 1 10 <node-id> 1
```

For each: confirm an immediate initial report, a new report when the real
reading changes (e.g. breathe near the sensor for CO2/PM), and a periodic
report even without a change (proves the subscription itself is alive,
independent of the measurement service's own ~7 s notify heartbeat). Running
this against one `AttributeAccessInterface`-backed cluster
(`pm25concentrationmeasurement`, `airquality`) and one plain-ember cluster
(`temperaturemeasurement`) covers both attribute-storage code paths in
`air_quality_matter_adapter.cpp`.

Record one full expected-vs-observed pass (values and a log excerpt) as part
of the change's definition of done.
