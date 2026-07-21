/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Sen66 {

/* 16-bit SEN66 I2C commands, sent big-endian on the wire. */
enum class Command : uint16_t {
    StartMeasurement = 0x0021,
    StopMeasurement = 0x0104,
    GetDataReady = 0x0202,
    ReadMeasuredValues = 0x0300,
    GetSerialNumber = 0xD033,
    DeviceReset = 0xD304,
};

/* Wire layout: every data word is 2 payload bytes followed by 1 CRC byte. */
constexpr size_t kWordSize = 3;
constexpr size_t kMeasurementWords = 9;
constexpr size_t kMeasurementBytes = kMeasurementWords * kWordSize; // 27
constexpr size_t kSerialWords = 16;
constexpr size_t kSerialBytes = kSerialWords * kWordSize;           // 48
constexpr size_t kSerialChars = kSerialWords * 2 + 1;               // 32 + NUL
constexpr size_t kDataReadyBytes = kWordSize;                       // 3

/*
 * Values the sensor reports when a channel has no valid reading. Unsigned
 * channels use 0xFFFF; signed channels use 0x7FFF. These clear the matching
 * validity bit and must never be surfaced as a real value (least of all zero).
 */
constexpr uint16_t kUnavailableUnsigned = 0xFFFF;
constexpr int16_t kUnavailableSigned = 0x7FFF;

/* Validity bits for Measurement.valid. */
enum Field : uint16_t {
    kFieldPm1p0 = 1u << 0,
    kFieldPm2p5 = 1u << 1,
    kFieldPm4p0 = 1u << 2,
    kFieldPm10 = 1u << 3,
    kFieldHumidity = 1u << 4,
    kFieldTemperature = 1u << 5,
    kFieldVocIndex = 1u << 6,
    kFieldNoxIndex = 1u << 7,
    kFieldCo2 = 1u << 8,
};

/*
 * One decoded measurement in the sensor's native fixed-point. Unit conversion
 * to Matter attributes is deferred to M4; consumers must check `valid` and
 * never treat a cleared field as zero.
 *
 *   field             unit        raw scale (raw / N)   signedness
 *   pm1p0..pm10       ug/m^3      10                    unsigned
 *   humidity          %RH         100                   signed
 *   temperature       degC        200                   signed
 *   voc/nox_index     index       10                    signed
 *   co2               ppm         1                     unsigned
 */
struct Measurement {
    uint16_t pm1p0;
    uint16_t pm2p5;
    uint16_t pm4p0;
    uint16_t pm10;
    int16_t humidity;
    int16_t temperature;
    int16_t voc_index;
    int16_t nox_index;
    uint16_t co2;
    uint16_t valid;
};

/* True when the Sensirion CRC of every word in `buf` (length words*3) matches. */
bool CheckWordsCrc(const uint8_t *buf, size_t words);

/*
 * Parse a 27-byte "read measured values" response. Returns false when any word
 * CRC fails (a transport error; retry is the measurement service's job). On
 * success, every channel is decoded with the correct signedness and its
 * validity bit is set only when the raw word is not the unavailable sentinel.
 */
bool ParseMeasurement(const uint8_t raw[kMeasurementBytes], Measurement &out);

/*
 * Parse a 48-byte serial-number response into a NUL-terminated ASCII string of
 * up to 32 characters. Returns false when any word CRC fails.
 */
bool ParseSerialNumber(const uint8_t raw[kSerialBytes], char out[kSerialChars]);

} // namespace Sen66
