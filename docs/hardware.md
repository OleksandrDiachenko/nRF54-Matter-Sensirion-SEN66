# Hardware

## Target configuration

- Board: `nrf54lm20dk/nrf54lm20a/cpuapp`
- Shield: `nrf7002eb2`
- Sensor: Sensirion SEN66
- I2C address: `0x6B`

The SEN66 wiring, verified in the prior I2C prototype, uses:

| SEN66 signal | nRF54LM20 DK pin |
| --- | --- |
| SDA | P1.11 |
| SCL | P1.12 |
| GND | GND |
| VDD | Sensor-specified supply |

The production DeviceTree overlay (added in `feature/sen66-driver`) lives in
`boards/nrf54lm20dk_nrf54lm20a_cpuapp.overlay`. It enables `i2c21` at
`I2C_BITRATE_STANDARD` (100 kHz) with the pins above and a `sen66@6b` node.
`i2c21` is the only serial instance the `nrf7002eb2` shield leaves free: `i2c22`
/ `spi22` drives the nRF70, and `uart30` is the console. P1.11/P1.12 are on
port 1 and do not clash with the shield (ports 3 and 0), so the overlay builds
together with `SHIELD=nrf7002eb2`.

## Network used during development

The development network is the Android hotspot `BUCKET`. The iPhone and the
accessory must join the same network during commissioning and local testing.
The network password is never committed, logged, or encoded in NFC.
