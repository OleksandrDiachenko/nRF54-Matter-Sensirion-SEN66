/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sen66_crc.h"

namespace Sen66 {

namespace {
constexpr uint8_t kPolynomial = 0x31;
constexpr uint8_t kInit = 0xFF;
} // namespace

uint8_t Crc(const uint8_t *data, size_t length) {
    uint8_t crc = kInit;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = static_cast<uint8_t>((crc << 1) ^ kPolynomial);
            } else {
                crc = static_cast<uint8_t>(crc << 1);
            }
        }
    }

    return crc;
}

} // namespace Sen66
