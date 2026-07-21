/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sen66_crc.h"

#include <zephyr/ztest.h>

ZTEST_SUITE(sen66_crc, NULL, NULL, NULL, NULL, NULL);

/* Canonical Sensirion reference: CRC-8(0xBEEF) == 0x92. */
ZTEST(sen66_crc, test_datasheet_vector) {
    const uint8_t word[] = {0xBE, 0xEF};
    zassert_equal(Sen66::Crc(word, sizeof(word)), 0x92, "Sensirion CRC(0xBEEF) must be 0x92");
}

ZTEST(sen66_crc, test_detects_bit_flip) {
    const uint8_t word[] = {0xBE, 0xEF};
    const uint8_t flipped[] = {0xBE, 0xEE};
    zassert_not_equal(Sen66::Crc(word, sizeof(word)), Sen66::Crc(flipped, sizeof(flipped)),
                      "CRC must change when a data bit flips");
}
