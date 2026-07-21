/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sen66_driver.h"

#include "sen66_crc.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sen66, LOG_LEVEL_INF);

namespace Sen66 {

namespace {

const struct i2c_dt_spec kDev = I2C_DT_SPEC_GET(DT_NODELABEL(sen66));

/*
 * SEN6x commands need no clock stretching: the master issues the command, waits
 * the execution time, then reads. Values are the datasheet maxima with margin.
 */
constexpr int kExecReadMs = 20;
constexpr int kExecStartMs = 50;
constexpr int kExecStopMs = 1000;

int SendCommand(Command cmd) {
    const uint16_t opcode = static_cast<uint16_t>(cmd);
    uint8_t buf[2] = {static_cast<uint8_t>(opcode >> 8), static_cast<uint8_t>(opcode & 0xFF)};
    return i2c_write_dt(&kDev, buf, sizeof(buf));
}

int ReadBytes(uint8_t *buf, size_t length) {
    return i2c_read_dt(&kDev, buf, length);
}

} // namespace

bool Init() {
    if (!device_is_ready(kDev.bus)) {
        LOG_ERR("SEN66 I2C bus %s not ready", kDev.bus->name);
        return false;
    }
    LOG_INF("SEN66 I2C ready on %s addr 0x%02x", kDev.bus->name, static_cast<unsigned>(kDev.addr));
    return true;
}

int ReadSerialNumber(char out[kSerialChars]) {
    int err = SendCommand(Command::GetSerialNumber);
    if (err) {
        return err;
    }
    k_msleep(kExecReadMs);

    uint8_t raw[kSerialBytes];
    err = ReadBytes(raw, sizeof(raw));
    if (err) {
        return err;
    }

    if (!ParseSerialNumber(raw, out)) {
        LOG_WRN("SEN66 serial-number CRC mismatch");
        return -EIO;
    }
    return 0;
}

int StartMeasurement() {
    int err = SendCommand(Command::StartMeasurement);
    if (err) {
        return err;
    }
    k_msleep(kExecStartMs);
    return 0;
}

int StopMeasurement() {
    int err = SendCommand(Command::StopMeasurement);
    if (err) {
        return err;
    }
    k_msleep(kExecStopMs);
    return 0;
}

bool IsDataReady() {
    if (SendCommand(Command::GetDataReady) != 0) {
        return false;
    }
    k_msleep(kExecReadMs);

    uint8_t raw[kDataReadyBytes];
    if (ReadBytes(raw, sizeof(raw)) != 0) {
        return false;
    }
    if (!CheckWordsCrc(raw, 1)) {
        LOG_WRN("SEN66 data-ready CRC mismatch");
        return false;
    }

    // The word is 0x0001 when a new measurement is ready; the flag is the low byte.
    return raw[1] == 0x01;
}

int ReadMeasurement(Measurement &out) {
    int err = SendCommand(Command::ReadMeasuredValues);
    if (err) {
        return err;
    }
    k_msleep(kExecReadMs);

    uint8_t raw[kMeasurementBytes];
    err = ReadBytes(raw, sizeof(raw));
    if (err) {
        return err;
    }

    if (!ParseMeasurement(raw, out)) {
        LOG_WRN("SEN66 measurement CRC mismatch");
        return -EIO;
    }
    return 0;
}

} // namespace Sen66
