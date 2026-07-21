/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Sen66 {

/*
 * Sensirion CRC-8: polynomial 0x31, initial value 0xFF, MSB-first, no final
 * XOR. Computed over each 2-byte data word; the sensor appends the result as a
 * third byte. This function has no Zephyr dependency so it is unit-tested on
 * the host.
 */
uint8_t Crc(const uint8_t *data, size_t length);

} // namespace Sen66
