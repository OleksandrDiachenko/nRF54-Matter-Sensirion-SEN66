/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "air_quality_policy.h"

#include <math.h>

#include <zephyr/ztest.h>

using namespace AirQualityEndpoint;

namespace {

bool NearlyEqual(float a, float b) {
    return fabsf(a - b) < 0.01f;
}

} // namespace

ZTEST_SUITE(air_quality_policy, NULL, NULL, NULL, NULL, NULL);

/* --- Unit converters --- */

ZTEST(air_quality_policy, test_co2_raw_to_ppm_is_pass_through) {
    zassert_true(NearlyEqual(Co2RawToPpm(0), 0.0f), NULL);
    zassert_true(NearlyEqual(Co2RawToPpm(800), 800.0f), NULL);
    zassert_true(NearlyEqual(Co2RawToPpm(5000), 5000.0f), NULL);
}

ZTEST(air_quality_policy, test_pm_raw_to_ugm3_divides_by_ten) {
    zassert_true(NearlyEqual(PmRawToUgm3(90), 9.0f), NULL);
    zassert_true(NearlyEqual(PmRawToUgm3(91), 9.1f), NULL);
    zassert_true(NearlyEqual(PmRawToUgm3(1500), 150.0f), NULL);
}

ZTEST(air_quality_policy, test_temperature_raw_to_matter_centidegc) {
    // raw is degC * 200; Matter wants degC * 100 (a plain halving).
    zassert_equal(TemperatureRawToMatterCentiDegC(4400), 2200, NULL); // 22.00 degC
    zassert_equal(TemperatureRawToMatterCentiDegC(0), 0, NULL);
    zassert_equal(TemperatureRawToMatterCentiDegC(-1000), -500, NULL); // -5.00 degC
}

ZTEST(air_quality_policy, test_humidity_raw_to_matter_centipercent_is_pass_through) {
    // raw is already %RH * 100, matching Matter's resolution directly.
    zassert_equal(HumidityRawToMatterCentiPercent(4550), 4550, NULL); // 45.50 %RH
    zassert_equal(HumidityRawToMatterCentiPercent(0), 0, NULL);
}

/* --- Overall Air Quality aggregation --- */

ZTEST(air_quality_policy, test_all_channels_good_yields_good) {
    PolicyInputs in;
    in.co2Valid = true;
    in.co2Raw = 400; // 400 ppm
    in.pm2p5Valid = true;
    in.pm2p5Raw = 50; // 5.0 ug/m3
    in.pm10Valid = true;
    in.pm10Raw = 200; // 20.0 ug/m3

    zassert_equal(EvaluateAirQuality(in), AirQualityLevel::kGood, NULL);
}

ZTEST(air_quality_policy, test_no_valid_channels_yields_unknown) {
    PolicyInputs in;

    zassert_equal(EvaluateAirQuality(in), AirQualityLevel::kUnknown,
                 "no valid channel must never default to Good or any other real category");
}

ZTEST(air_quality_policy, test_single_valid_channel_drives_result) {
    PolicyInputs in;
    in.pm10Valid = true;
    in.pm10Raw = 2000; // 200.0 ug/m3 -> Moderate (155.5-254.4 range)
    // co2Valid and pm2p5Valid stay false.

    zassert_equal(EvaluateAirQuality(in), AirQualityLevel::kModerate,
                 "invalid channels must not pull the result toward Good");
}

ZTEST(air_quality_policy, test_worst_channel_wins) {
    PolicyInputs in;
    in.co2Valid = true;
    in.co2Raw = 400; // Good
    in.pm2p5Valid = true;
    in.pm2p5Raw = 1000; // 100.0 ug/m3 -> Poor
    in.pm10Valid = true;
    in.pm10Raw = 200; // Good

    zassert_equal(EvaluateAirQuality(in), AirQualityLevel::kPoor, NULL);
}

ZTEST(air_quality_policy, test_multiple_elevated_channels_worst_of_the_set_wins) {
    PolicyInputs in;
    in.co2Valid = true;
    in.co2Raw = 3000; // 3000 ppm -> VeryPoor
    in.pm2p5Valid = true;
    in.pm2p5Raw = 200; // 20.0 ug/m3 -> Fair
    in.pm10Valid = true;
    in.pm10Raw = 200; // Good

    zassert_equal(EvaluateAirQuality(in), AirQualityLevel::kVeryPoor, NULL);
}

ZTEST(air_quality_policy, test_extreme_value_clamps_to_extremely_poor) {
    PolicyInputs in;
    in.co2Valid = true;
    in.co2Raw = 60000; // far above every breakpoint

    zassert_equal(EvaluateAirQuality(in), AirQualityLevel::kExtremelyPoor, NULL);
}

/* --- Boundary behavior: <= upper bound stays in the lower category --- */

ZTEST(air_quality_policy, test_co2_good_fair_boundary_is_inclusive_on_good) {
    zassert_equal(ClassifyChannel(800.0f, kCo2BreakpointsPpm), AirQualityLevel::kGood, NULL);
    zassert_equal(ClassifyChannel(800.01f, kCo2BreakpointsPpm), AirQualityLevel::kFair, NULL);
}

ZTEST(air_quality_policy, test_pm2p5_good_fair_boundary_is_inclusive_on_good) {
    zassert_equal(ClassifyChannel(9.0f, kPm2p5BreakpointsUgm3), AirQualityLevel::kGood, NULL);
    zassert_equal(ClassifyChannel(9.1f, kPm2p5BreakpointsUgm3), AirQualityLevel::kFair, NULL);
}

ZTEST(air_quality_policy, test_pm10_poor_verypoor_boundary_is_inclusive_on_poor) {
    zassert_equal(ClassifyChannel(354.0f, kPm10BreakpointsUgm3), AirQualityLevel::kPoor, NULL);
    zassert_equal(ClassifyChannel(354.01f, kPm10BreakpointsUgm3), AirQualityLevel::kVeryPoor, NULL);
}
