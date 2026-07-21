/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sen66_protocol.h"

#include "sen66_crc.h"

namespace Sen66 {

namespace {

uint16_t Word(const uint8_t *w) {
    return static_cast<uint16_t>((static_cast<uint16_t>(w[0]) << 8) | w[1]);
}

} // namespace

bool CheckWordsCrc(const uint8_t *buf, size_t words) {
    for (size_t i = 0; i < words; i++) {
        const uint8_t *w = &buf[i * kWordSize];
        if (Crc(w, 2) != w[2]) {
            return false;
        }
    }
    return true;
}

bool ParseMeasurement(const uint8_t raw[kMeasurementBytes], Measurement &out) {
    if (!CheckWordsCrc(raw, kMeasurementWords)) {
        return false;
    }

    const uint16_t pm1p0 = Word(&raw[0 * kWordSize]);
    const uint16_t pm2p5 = Word(&raw[1 * kWordSize]);
    const uint16_t pm4p0 = Word(&raw[2 * kWordSize]);
    const uint16_t pm10 = Word(&raw[3 * kWordSize]);
    const uint16_t humidity = Word(&raw[4 * kWordSize]);
    const uint16_t temperature = Word(&raw[5 * kWordSize]);
    const uint16_t voc = Word(&raw[6 * kWordSize]);
    const uint16_t nox = Word(&raw[7 * kWordSize]);
    const uint16_t co2 = Word(&raw[8 * kWordSize]);

    out = Measurement{};
    out.pm1p0 = pm1p0;
    out.pm2p5 = pm2p5;
    out.pm4p0 = pm4p0;
    out.pm10 = pm10;
    out.humidity = static_cast<int16_t>(humidity);
    out.temperature = static_cast<int16_t>(temperature);
    out.voc_index = static_cast<int16_t>(voc);
    out.nox_index = static_cast<int16_t>(nox);
    out.co2 = co2;

    if (pm1p0 != kUnavailableUnsigned) {
        out.valid |= kFieldPm1p0;
    }
    if (pm2p5 != kUnavailableUnsigned) {
        out.valid |= kFieldPm2p5;
    }
    if (pm4p0 != kUnavailableUnsigned) {
        out.valid |= kFieldPm4p0;
    }
    if (pm10 != kUnavailableUnsigned) {
        out.valid |= kFieldPm10;
    }
    if (out.humidity != kUnavailableSigned) {
        out.valid |= kFieldHumidity;
    }
    if (out.temperature != kUnavailableSigned) {
        out.valid |= kFieldTemperature;
    }
    if (out.voc_index != kUnavailableSigned) {
        out.valid |= kFieldVocIndex;
    }
    if (out.nox_index != kUnavailableSigned) {
        out.valid |= kFieldNoxIndex;
    }
    if (co2 != kUnavailableUnsigned) {
        out.valid |= kFieldCo2;
    }

    return true;
}

bool ParseSerialNumber(const uint8_t raw[kSerialBytes], char out[kSerialChars]) {
    if (!CheckWordsCrc(raw, kSerialWords)) {
        return false;
    }

    size_t n = 0;
    for (size_t i = 0; i < kSerialWords; i++) {
        const uint8_t *w = &raw[i * kWordSize];
        out[n++] = static_cast<char>(w[0]);
        out[n++] = static_cast<char>(w[1]);
    }
    out[n] = '\0';

    // The serial number is a NUL-terminated ASCII string padded with zeros, so
    // any embedded NUL already terminates it; the final terminator is a guard.
    return true;
}

} // namespace Sen66
