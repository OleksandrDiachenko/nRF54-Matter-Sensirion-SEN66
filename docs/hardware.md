# Hardware

## Target configuration

- Board: `nrf54lm20dk/nrf54lm20a/cpuapp`
- Shield: `nrf7002eb2`
- Sensor: Sensirion SEN66
- I2C address: `0x6B`

The preliminary SEN66 wiring, verified in the prior I2C prototype, uses:

| SEN66 signal | nRF54LM20 DK pin |
| --- | --- |
| SDA | P1.11 |
| SCL | P1.12 |
| GND | GND |
| VDD | Sensor-specified supply |

The production DeviceTree overlay is added with the SEN66 driver feature. It
must be built together with the `nrf7002eb2` shield configuration and verified
on hardware before it is accepted.

## Network used during development

The development network is the Android hotspot `BUCKET`. The iPhone and the
accessory must join the same network during commissioning and local testing.
The network password is never committed, logged, or encoded in NFC.
