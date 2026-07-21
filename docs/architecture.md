# Architecture

## Goal

The accessory reads air-quality data from an SEN66 over I2C and publishes
standards-compliant Matter attributes over Wi-Fi.

## Boundaries

```text
SEN66 I2C transport -> SEN66 protocol/parser -> measurement service -> Matter endpoint
```

`main.cpp` is limited to application startup. It does not contain I2C framing,
sensor parsing, measurement policy, or Matter attribute logic.

The sensor parser produces a typed measurement object with a validity mask. A
missing value is never converted to zero. The measurement service owns polling,
retry, and backoff. The Matter adapter is the only component that writes Matter
attributes and runs those writes in the Matter stack context.

## Matter data model

The endpoint uses the Matter Air Quality Sensor device type. Initial standard
clusters are:

| SEN66 measurement | Matter cluster | Unit |
| --- | --- | --- |
| Temperature | Temperature Measurement | 0.01 °C |
| Relative humidity | Relative Humidity Measurement | 0.01 %RH |
| CO2 | Carbon Dioxide Concentration Measurement | ppm |
| PM1.0 | PM1 Concentration Measurement | µg/m³ |
| PM2.5 | PM2.5 Concentration Measurement | µg/m³ |
| PM10 | PM10 Concentration Measurement | µg/m³ |
| Overall air quality | Air Quality | Matter enum |

PM4.0 has no matching standard Matter cluster. SEN66 VOC and NOx values are
indexes, not TVOC or NO2 concentrations; they must not be published in those
concentration clusters. They may inform the documented overall-air-quality
policy in a later milestone.

## Transport and commissioning

The device uses Matter over Wi-Fi with the nRF7002-EB II. Bluetooth LE is used
only for commissioning rendezvous. NFC carries the Matter onboarding payload;
the Matter commissioner transfers Wi-Fi credentials securely during
commissioning. No Wi-Fi credential is stored in the NFC payload.
