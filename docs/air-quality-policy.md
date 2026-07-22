# Overall air-quality policy

Milestone M4 publishes a single Matter `AirQuality` enum attribute
(`AirQualityEnum`: Unknown, Good, Fair, Moderate, Poor, VeryPoor,
ExtremelyPoor) alongside the individual concentration measurements. This
document defines and cites the policy that computes it, per
[architecture.md](architecture.md).

## Scope

The policy classifies exactly three channels: **CO2, PM2.5, and PM10** - the
channels the roadmap names for M4. Two channels are deliberately excluded:

- **PM1.0** is published as its own Matter cluster but does not feed this
  policy. No standard air-quality index defines PM1.0 breakpoints; folding it
  in would mean inventing thresholds instead of citing them.
- **VOC index and NOx index** are Sensirion's proprietary index scale (not a
  TVOC or NO2 concentration - see [architecture.md](architecture.md)), so they
  cannot be run through a concentration-based standard, and are not published
  to Matter at all (see architecture.md for why).

## Source, code location

Implemented in `src/air_quality_endpoint/air_quality_policy.{h,cpp}`
(`AirQualityEndpoint::EvaluateAirQuality`), pure and host-testable like
`measurement_policy.cpp` - no Zephyr or Matter dependency. Unit-tested in
`tests/air_quality_endpoint/`.

## Per-channel breakpoints

Each channel is classified independently into one of six categories (Good
through ExtremelyPoor); each bound below is the **inclusive upper edge** of
that category, i.e. a value equal to the bound stays in the lower category.

### Carbon dioxide (CO2), ppm

| Good | Fair | Moderate | Poor | VeryPoor | ExtremelyPoor |
| --- | --- | --- | --- | --- | --- |
| ≤ 800 | ≤ 1000 | ≤ 1500 | ≤ 2500 | ≤ 5000 | > 5000 |

There is no single official "CO2 AQI" the way there is for particulates, so
these bounds synthesize three cited sources:

- **800 / 1000 ppm** - ANSI/ASHRAE Standard 62.1-2022 recommends a
  steady-state indoor CO2 concentration no more than ~700 ppm above outdoor
  air (typically 300-500 ppm), i.e. roughly a 1000-1100 ppm ceiling for
  "acceptable" ventilation; 800 ppm is used as the Good/Fair edge to stay
  inside that guidance with margin.
- **1000 / 1500 ppm** - Allen et al., *"Associations of Cognitive Function
  Scores with Carbon Dioxide, Ventilation, and Volatile Organic Compound
  Exposures in Office Workers,"* Environmental Health Perspectives 124(6),
  2016 (the Harvard T.H. Chan "CogFx" study): a controlled-exposure study
  that measured a ~15% decline in cognitive function scores at ~950 ppm and a
  ~50% decline at ~1400 ppm relative to ~550 ppm baseline, which places the
  Fair/Moderate and Moderate/Poor edges either side of that documented
  decline.
- **5000 ppm** - OSHA 29 CFR 1910.1000 Table Z-1 permissible exposure limit
  (PEL) and the NIOSH recommended exposure limit (REL), both an 8-hour
  time-weighted-average occupational exposure limit of 5000 ppm. VeryPoor/
  ExtremelyPoor is set at this recognized occupational safety limit, not a
  comfort threshold.

### PM2.5, µg/m³ (instantaneous reading)

| Good | Fair | Moderate | Poor | VeryPoor | ExtremelyPoor |
| --- | --- | --- | --- | --- | --- |
| ≤ 9.0 | ≤ 35.4 | ≤ 55.4 | ≤ 125.4 | ≤ 225.4 | > 225.4 |

Source: US EPA Air Quality Index, 2024 revision (89 FR 16202, February 7,
2024; EPA raised the annual PM2.5 NAAQS and lowered the "Good" AQI breakpoint
from 12.0 to 9.0 µg/m³). EPA's breakpoints are defined for a 24-hour average;
SEN66 reports an instantaneous reading, so this policy applies the same
breakpoints to the latest reading as a live approximation, matching common
practice for low-cost sensor displays (e.g. PurpleAir, AirNow's "NowCast" for
current conditions). Mapping to Matter's six-level `AirQualityEnum` is 1:1
with EPA's six named categories: Good→Good, Moderate→Fair, Unhealthy for
Sensitive Groups→Moderate, Unhealthy→Poor, Very Unhealthy→VeryPoor,
Hazardous→ExtremelyPoor.

### PM10, µg/m³ (instantaneous reading)

| Good | Fair | Moderate | Poor | VeryPoor | ExtremelyPoor |
| --- | --- | --- | --- | --- | --- |
| ≤ 54 | ≤ 154 | ≤ 254 | ≤ 354 | ≤ 424 | > 424 |

Source: US EPA Air Quality Index PM10 breakpoints (unchanged by the 2024
PM2.5 revision). Same instantaneous-vs-24-hour-average caveat and category
mapping as PM2.5 above.

## Aggregation rule

`Good < Fair < Moderate < Poor < VeryPoor < ExtremelyPoor`. The overall
`AirQuality` value is **the worst category among the currently-valid
channels**; a channel whose `Sen66::Field` validity bit is clear (sensor
warming up, disconnected, or mid-backoff per
[measurement-service.md](measurement-service.md)) is excluded from
aggregation entirely, not treated as Good. If zero channels are valid, the
result is `Unknown` - never a fabricated "Good" default.

## Why "worst wins"

A single poor reading (e.g. a CO2 spike from an unventilated room) should not
be masked by two otherwise-clean channels. This mirrors how the standard US
EPA AQI itself is computed for multi-pollutant days: the reported AQI is the
maximum of the per-pollutant sub-indices, not an average.
