/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "air_quality_policy.h"

#include <algorithm>

namespace AirQualityEndpoint {

float Co2RawToPpm(uint16_t raw) {
    return static_cast<float>(raw);
}

float PmRawToUgm3(uint16_t raw) {
    return static_cast<float>(raw) / 10.0f;
}

int16_t TemperatureRawToMatterCentiDegC(int16_t raw) {
    // raw is degC * 200; Matter's Temperature Measurement wants degC * 100.
    return static_cast<int16_t>(static_cast<int32_t>(raw) * 100 / 200);
}

uint16_t HumidityRawToMatterCentiPercent(int16_t raw) {
    // raw is already %RH * 100, matching Matter's Relative Humidity
    // Measurement resolution directly; only the signedness differs.
    return static_cast<uint16_t>(raw);
}

AirQualityLevel ClassifyChannel(float value, const ChannelBreakpoints &bp) {
    if (value <= bp.good) {
        return AirQualityLevel::kGood;
    }
    if (value <= bp.fair) {
        return AirQualityLevel::kFair;
    }
    if (value <= bp.moderate) {
        return AirQualityLevel::kModerate;
    }
    if (value <= bp.poor) {
        return AirQualityLevel::kPoor;
    }
    if (value <= bp.veryPoor) {
        return AirQualityLevel::kVeryPoor;
    }
    return AirQualityLevel::kExtremelyPoor;
}

AirQualityLevel EvaluateAirQuality(const PolicyInputs &inputs) {
    AirQualityLevel worst = AirQualityLevel::kUnknown;
    bool anyValid = false;

    auto considerChannel = [&](float value, const ChannelBreakpoints &bp) {
        anyValid = true;
        AirQualityLevel level = ClassifyChannel(value, bp);
        if (static_cast<uint8_t>(level) > static_cast<uint8_t>(worst)) {
            worst = level;
        }
    };

    if (inputs.co2Valid) {
        considerChannel(Co2RawToPpm(inputs.co2Raw), kCo2BreakpointsPpm);
    }
    if (inputs.pm2p5Valid) {
        considerChannel(PmRawToUgm3(inputs.pm2p5Raw), kPm2p5BreakpointsUgm3);
    }
    if (inputs.pm10Valid) {
        considerChannel(PmRawToUgm3(inputs.pm10Raw), kPm10BreakpointsUgm3);
    }

    return anyValid ? worst : AirQualityLevel::kUnknown;
}

} // namespace AirQualityEndpoint
