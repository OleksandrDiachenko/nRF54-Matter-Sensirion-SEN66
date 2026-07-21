/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sen66_crc.h"
#include "sen66_protocol.h"

#include <string.h>

#include <zephyr/ztest.h>

using namespace Sen66;

namespace {

constexpr uint16_t kAllValid = kFieldPm1p0 | kFieldPm2p5 | kFieldPm4p0 | kFieldPm10 |
                               kFieldHumidity | kFieldTemperature | kFieldVocIndex |
                               kFieldNoxIndex | kFieldCo2;

void PutWord(uint8_t *dst, uint16_t value) {
    dst[0] = static_cast<uint8_t>(value >> 8);
    dst[1] = static_cast<uint8_t>(value & 0xFF);
    dst[2] = Crc(dst, 2);
}

void BuildFrame(uint8_t raw[kMeasurementBytes], const uint16_t words[kMeasurementWords]) {
    for (size_t i = 0; i < kMeasurementWords; i++) {
        PutWord(&raw[i * kWordSize], words[i]);
    }
}

/* A plausible, fully-valid frame; individual tests override one word. */
void PlausibleWords(uint16_t words[kMeasurementWords]) {
    words[0] = 100;  // PM1.0  = 10.0 ug/m3
    words[1] = 150;  // PM2.5  = 15.0
    words[2] = 180;  // PM4.0  = 18.0
    words[3] = 200;  // PM10   = 20.0
    words[4] = 4550; // RH     = 45.50 %
    words[5] = 4400; // Temp   = 22.00 degC
    words[6] = 120;  // VOC    = 12.0
    words[7] = 10;   // NOx    = 1.0
    words[8] = 800;  // CO2    = 800 ppm
}

void BuildSerial(uint8_t raw[kSerialBytes], const char *text) {
    char padded[kSerialWords * 2] = {0};
    strncpy(padded, text, sizeof(padded));
    for (size_t i = 0; i < kSerialWords; i++) {
        const uint16_t value = static_cast<uint16_t>(
            (static_cast<uint8_t>(padded[2 * i]) << 8) | static_cast<uint8_t>(padded[2 * i + 1]));
        PutWord(&raw[i * kWordSize], value);
    }
}

} // namespace

ZTEST_SUITE(sen66_parser, NULL, NULL, NULL, NULL, NULL);

ZTEST(sen66_parser, test_all_valid_decode) {
    uint16_t words[kMeasurementWords];
    PlausibleWords(words);
    uint8_t raw[kMeasurementBytes];
    BuildFrame(raw, words);

    Measurement m;
    zassert_true(ParseMeasurement(raw, m), "plausible frame must parse");
    zassert_equal(m.valid, kAllValid, "every channel should be valid");
    zassert_equal(m.pm1p0, 100, NULL);
    zassert_equal(m.pm10, 200, NULL);
    zassert_equal(m.humidity, 4550, NULL);
    zassert_equal(m.temperature, 4400, NULL);
    zassert_equal(m.voc_index, 120, NULL);
    zassert_equal(m.co2, 800, NULL);
}

ZTEST(sen66_parser, test_temperature_negative_is_signed) {
    uint16_t words[kMeasurementWords];
    PlausibleWords(words);
    words[5] = static_cast<uint16_t>(-2000); // -10.00 degC (raw / 200)
    uint8_t raw[kMeasurementBytes];
    BuildFrame(raw, words);

    Measurement m;
    zassert_true(ParseMeasurement(raw, m), NULL);
    zassert_equal(m.temperature, -2000, "signed temperature must decode negative");
    zassert_true(m.valid & kFieldTemperature, "negative value is still valid");
}

ZTEST(sen66_parser, test_pm_above_int16_is_unsigned) {
    uint16_t words[kMeasurementWords];
    PlausibleWords(words);
    words[1] = 40000; // > INT16_MAX; signed decoding would be negative
    uint8_t raw[kMeasurementBytes];
    BuildFrame(raw, words);

    Measurement m;
    zassert_true(ParseMeasurement(raw, m), NULL);
    zassert_equal(m.pm2p5, 40000, "PM channels must decode as unsigned");
    zassert_true(m.valid & kFieldPm2p5, NULL);
}

ZTEST(sen66_parser, test_co2_above_int16_is_unsigned) {
    uint16_t words[kMeasurementWords];
    PlausibleWords(words);
    words[8] = 50000; // e.g. saturated CO2 reading
    uint8_t raw[kMeasurementBytes];
    BuildFrame(raw, words);

    Measurement m;
    zassert_true(ParseMeasurement(raw, m), NULL);
    zassert_equal(m.co2, 50000, "CO2 must decode as unsigned");
    zassert_true(m.valid & kFieldCo2, NULL);
}

ZTEST(sen66_parser, test_unsigned_sentinel_marks_invalid) {
    uint16_t words[kMeasurementWords];
    PlausibleWords(words);
    words[0] = kUnavailableUnsigned; // 0xFFFF on an unsigned channel
    uint8_t raw[kMeasurementBytes];
    BuildFrame(raw, words);

    Measurement m;
    zassert_true(ParseMeasurement(raw, m), "CRC is still valid, so the frame parses");
    zassert_false(m.valid & kFieldPm1p0, "0xFFFF must clear the PM1.0 validity bit");
    zassert_true(m.valid & kFieldPm2p5, "other channels stay valid");
}

ZTEST(sen66_parser, test_signed_sentinel_marks_invalid) {
    uint16_t words[kMeasurementWords];
    PlausibleWords(words);
    words[5] = static_cast<uint16_t>(kUnavailableSigned); // 0x7FFF on temperature
    uint8_t raw[kMeasurementBytes];
    BuildFrame(raw, words);

    Measurement m;
    zassert_true(ParseMeasurement(raw, m), NULL);
    zassert_false(m.valid & kFieldTemperature, "0x7FFF must clear the temperature bit");
    zassert_true(m.valid & kFieldHumidity, "other channels stay valid");
}

ZTEST(sen66_parser, test_bad_crc_rejects_whole_frame) {
    uint16_t words[kMeasurementWords];
    PlausibleWords(words);
    uint8_t raw[kMeasurementBytes];
    BuildFrame(raw, words);
    raw[2] ^= 0xFF; // corrupt the first word's CRC byte

    Measurement m;
    zassert_false(ParseMeasurement(raw, m), "a single bad word CRC must reject the frame");
}

ZTEST(sen66_parser, test_serial_number_ascii) {
    uint8_t raw[kSerialBytes];
    BuildSerial(raw, "SEN66-TEST");

    char out[kSerialChars];
    zassert_true(ParseSerialNumber(raw, out), "valid serial must parse");
    zassert_equal(strcmp(out, "SEN66-TEST"), 0, "serial mismatch: got '%s'", out);
}

ZTEST(sen66_parser, test_serial_bad_crc_rejected) {
    uint8_t raw[kSerialBytes];
    BuildSerial(raw, "ABCDEF");
    raw[2] ^= 0xFF;

    char out[kSerialChars];
    zassert_false(ParseSerialNumber(raw, out), "corrupt serial CRC must be rejected");
}
