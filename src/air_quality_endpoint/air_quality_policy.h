/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "sen66/sen66_protocol.h"

#include <stdint.h>

namespace AirQualityEndpoint {

/*
 * Mirrors chip::app::Clusters::AirQuality::AirQualityEnum's values exactly
 * (Unknown=0 .. ExtremelyPoor=6) without including any CHIP header, so this
 * file stays host-testable. The adapter casts between the two.
 */
enum class AirQualityLevel : uint8_t {
    kUnknown = 0,
    kGood = 1,
    kFair = 2,
    kModerate = 3,
    kPoor = 4,
    kVeryPoor = 5,
    kExtremelyPoor = 6,
};

/*
 * Raw SEN66 fixed-point -> Matter engineering unit converters. The caller
 * must check the matching Sen66::Field valid bit first; these never see the
 * sensor's unavailable sentinels and perform no validity handling themselves.
 */
float Co2RawToPpm(uint16_t raw);
float PmRawToUgm3(uint16_t raw);
int16_t TemperatureRawToMatterCentiDegC(int16_t raw);
uint16_t HumidityRawToMatterCentiPercent(int16_t raw);

/*
 * Per-channel category boundaries, in the channel's native engineering unit.
 * Each field is the inclusive upper bound of that category; a value above
 * `veryPoor` falls in ExtremelyPoor. See docs/air-quality-policy.md for the
 * cited source of every default value below - do not tune these without
 * updating that citation.
 */
struct ChannelBreakpoints {
    float good;
    float fair;
    float moderate;
    float poor;
    float veryPoor;
};

/* CO2, ppm. Source: docs/air-quality-policy.md "Carbon dioxide (CO2)". */
inline constexpr ChannelBreakpoints kCo2BreakpointsPpm{800.0f, 1000.0f, 1500.0f, 2500.0f, 5000.0f};

/* PM2.5, ug/m3, US EPA AQI 2024 revision. See docs/air-quality-policy.md. */
inline constexpr ChannelBreakpoints kPm2p5BreakpointsUgm3{9.0f, 35.4f, 55.4f, 125.4f, 225.4f};

/* PM10, ug/m3, US EPA AQI. See docs/air-quality-policy.md. */
inline constexpr ChannelBreakpoints kPm10BreakpointsUgm3{54.0f, 154.0f, 254.0f, 354.0f, 424.0f};

/* Classify a single already-converted engineering-unit value. */
AirQualityLevel ClassifyChannel(float value, const ChannelBreakpoints &bp);

/*
 * Inputs to the overall Air Quality policy. Deliberately CO2/PM2.5/PM10 only:
 * PM1 is published as its own Matter cluster but does not feed this policy,
 * and SEN66's VOC/NOx are indexes, not concentrations - see
 * docs/architecture.md and docs/air-quality-policy.md for why they are
 * excluded from M4's policy.
 */
struct PolicyInputs {
    bool co2Valid = false;
    uint16_t co2Raw = 0;
    bool pm2p5Valid = false;
    uint16_t pm2p5Raw = 0;
    bool pm10Valid = false;
    uint16_t pm10Raw = 0;
};

/*
 * Overall Air Quality = the worst category among currently-valid channels;
 * kUnknown when no channel is valid. Never reads an invalid channel's raw
 * value.
 */
AirQualityLevel EvaluateAirQuality(const PolicyInputs &inputs);

} // namespace AirQualityEndpoint
