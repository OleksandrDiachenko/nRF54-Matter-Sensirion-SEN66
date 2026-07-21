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

## SEN66 hardware smoke test

With the firmware flashed and the sensor wired, use the Matter shell (vcom0) to
exercise the real sensor before any Matter integration:

```console
uart:~$ sen66 serial      # prints the sensor serial number
uart:~$ sen66 read        # prints one measurement; unavailable channels show <n/a>
```
