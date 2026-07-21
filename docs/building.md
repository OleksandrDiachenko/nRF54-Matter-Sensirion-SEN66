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

## Baseline acceptance criteria

1. The image builds without configuration or linker errors.
2. On first boot the serial log prints a Matter onboarding payload.
3. The device is commissionable with QR/manual onboarding to the `BUCKET`
   hotspot.
4. The device reconnects after reboot.
5. NFC exposes the same Matter onboarding payload. It does not include Wi-Fi
   credentials; the commissioner sends those during Matter commissioning.
