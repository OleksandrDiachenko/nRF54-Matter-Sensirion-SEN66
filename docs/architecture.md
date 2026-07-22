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

Endpoint 1 uses the Matter Air Quality Sensor device type
(`MA-air-quality-sensor`, `0x002C`), defined in
`src/default_zap/air_quality_sensor.zap`. Clusters:

| SEN66 measurement | Matter cluster | Unit |
| --- | --- | --- |
| Temperature | Temperature Measurement | 0.01 °C |
| Relative humidity | Relative Humidity Measurement | 0.01 %RH |
| CO2 | Carbon Dioxide Concentration Measurement | ppm |
| PM1.0 | PM1 Concentration Measurement | µg/m³ |
| PM2.5 | PM2.5 Concentration Measurement | µg/m³ |
| PM10 | PM10 Concentration Measurement | µg/m³ |
| Overall air quality | Air Quality | Matter enum |

Endpoint 2 uses the Matter Temperature Sensor device type (`MA-tempsensor`,
`0x0302`), with its own Identify, Descriptor, and Temperature Measurement
clusters - it republishes the same reading endpoint 1 already carries. This
exists purely because Apple Home's Air Quality Sensor device type UI renders
no tile at all for a Temperature Measurement cluster on that device type
(confirmed on hardware, see [apple-home.md](apple-home.md)); a standalone
Temperature Sensor endpoint is the only way to get a native Home tile for
it - confirmed working on hardware after re-commissioning.

Plus the mandatory Descriptor and Identify clusters on endpoint 1. PM4.0 has
no matching standard Matter cluster at all, so it is not published anywhere.

SEN66's VOC and NOx values are indexes, not TVOC or NO2 concentrations, and
are not published to Matter at all - no Matter `MeasurementUnit` honestly
describes an index, and mapping them onto the closest available clusters
(`TotalVolatileOrganicCompoundsConcentrationMeasurement`,
`NitrogenDioxideConcentrationMeasurement` via its `LevelIndication` feature)
was tried and reverted: Apple Home's UI did not render a tile for either
regardless, so publishing them added complexity without the payoff this
project cares about (a Home-visible reading). They remain readable only via
`sen66 latest`/`sen66 read` (see [sen66-driver.md](sen66-driver.md)) and are
excluded from the CO2/PM2.5/PM10 air-quality policy below for the same
reason - see [air-quality-policy.md](air-quality-policy.md).

Two attribute-storage patterns are in play, matching how each cluster is
implemented in the CHIP SDK:

- **Temperature Measurement / Relative Humidity Measurement** are plain
  ember RAM-backed attributes, written with the generated
  `Attributes::MeasuredValue::Set()`/`SetNull()` accessors.
- **Air Quality and every Concentration Measurement cluster** are code-driven
  `AttributeAccessInterface` instances (`AirQuality::Instance`,
  `ConcentrationMeasurement::Instance<...>`), written with
  `UpdateAirQuality()`/`SetMeasuredValue()`. Only the `NumericMeasurement`
  feature is enabled on the concentration clusters - SEN66 provides no level
  thresholds, peak, or average statistics, so those optional features stay
  off rather than publishing fabricated data.

`src/air_quality_endpoint/air_quality_matter_adapter.cpp` owns every
instance across both endpoints and is the only code that calls into any of
them.

## Transport and commissioning

The device uses Matter over Wi-Fi with the nRF7002-EB II. Bluetooth LE is used
only for commissioning rendezvous. NFC carries the Matter onboarding payload;
the Matter commissioner transfers Wi-Fi credentials securely during
commissioning. No Wi-Fi credential is stored in the NFC payload.
