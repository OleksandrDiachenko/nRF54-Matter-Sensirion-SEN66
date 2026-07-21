/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "sen66_protocol.h"

namespace Sen66 {

/*
 * SEN66 I2C transport. This is the only translation unit that talks to Zephyr
 * I2C; the command framing, timing, and per-word CRC checks live here, while
 * the pure decoding stays in sen66_protocol / sen66_crc.
 *
 * All entry points that return int follow the Zephyr convention: 0 on success,
 * a negative errno on failure.
 */

/* Check that the I2C bus and the sen66 node are ready. Logs the outcome. */
bool Init();

/* Read the sensor serial number into a NUL-terminated ASCII string. */
int ReadSerialNumber(char out[kSerialChars]);

/* Start continuous measurement mode. Values become valid after the warm-up. */
int StartMeasurement();

/* Stop measurement mode and return the sensor to idle. */
int StopMeasurement();

/* True when a fresh measurement is available to read. */
bool IsDataReady();

/* Read and parse one measurement frame. */
int ReadMeasurement(Measurement &out);

} // namespace Sen66
