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

## Flashing every sysbuild domain

This project has three independently flashable images. On NCS 3.4, `west
flash` may prompt to choose one domain; selecting only the application does
not update MCUboot. Flash the domains in this exact order, using the DK J-Link
serial number (`001051803904` on this workstation):

```console
west flash -d build/nrf54lm20-wifi --domain mcuboot --no-rebuild --dev-id 001051803904
west flash -d build/nrf54lm20-wifi --domain matter_factory_data --no-rebuild --dev-id 001051803904
west flash -d build/nrf54lm20-wifi --domain nRF54-Matter-Sensirion-SEN66 --no-rebuild --dev-id 001051803904
```

The application must be flashed as the generated **signed** image. Never
select `zephyr.hex` manually in a programmer: it is not a bootable MCUboot
image. The `nRF54-Matter-Sensirion-SEN66` sysbuild domain automatically uses
`zephyr.signed.hex`.

The MCUboot overlay in `sysbuild/mcuboot/` deliberately shares the Matter
partition layout with the application (`slot0` at `0xD000`). Keep these files
in sync when changing partitions.

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
