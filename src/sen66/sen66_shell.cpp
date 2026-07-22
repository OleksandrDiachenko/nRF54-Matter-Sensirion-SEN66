/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sen66_driver.h"
#include "sen66_protocol.h"

#if defined(CONFIG_APP_MEASUREMENT_SERVICE)
#include "measurement_service/measurement_service.h"
#endif

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

/*
 * Diagnostic "sen66" shell command used as the M2/M3 hardware smoke test. It
 * exercises the driver and measurement service against the real sensor
 * without any Matter integration (that arrives in M4). Values print in human
 * units; a channel whose validity bit is clear prints "<n/a>" rather than a
 * misleading zero.
 */

namespace {

/* Poll for a fresh measurement for up to ~2 s to cover the first warm-up. */
constexpr int kDataReadyPollCount = 20;
constexpr int kDataReadyPollMs = 100;

void PrintUnsignedTenth(const struct shell *sh, const char *label, uint16_t raw, const char *unit) {
    shell_print(sh, "  %-12s %u.%u %s", label, static_cast<unsigned>(raw / 10),
                static_cast<unsigned>(raw % 10), unit);
}

/* Print `scaled` (value * frac_pow) as a signed decimal with the given resolution. */
void PrintSignedFixed(const struct shell *sh, const char *label, int32_t scaled, int frac_pow,
                      const char *unit) {
    int32_t whole = scaled / frac_pow;
    int32_t frac = scaled % frac_pow;
    if (frac < 0) {
        frac = -frac;
    }
    const char *sign = (scaled < 0 && whole == 0) ? "-" : "";
    if (frac_pow == 100) {
        shell_print(sh, "  %-12s %s%d.%02d %s", label, sign, whole, frac, unit);
    } else {
        shell_print(sh, "  %-12s %s%d.%d %s", label, sign, whole, frac, unit);
    }
}

void PrintNa(const struct shell *sh, const char *label) {
    shell_print(sh, "  %-12s <n/a>", label);
}

void PrintMeasurement(const struct shell *sh, const Sen66::Measurement &m) {
    using namespace Sen66;

    if (m.valid & kFieldPm1p0) {
        PrintUnsignedTenth(sh, "PM1.0", m.pm1p0, "ug/m3");
    } else {
        PrintNa(sh, "PM1.0");
    }
    if (m.valid & kFieldPm2p5) {
        PrintUnsignedTenth(sh, "PM2.5", m.pm2p5, "ug/m3");
    } else {
        PrintNa(sh, "PM2.5");
    }
    if (m.valid & kFieldPm4p0) {
        PrintUnsignedTenth(sh, "PM4.0", m.pm4p0, "ug/m3");
    } else {
        PrintNa(sh, "PM4.0");
    }
    if (m.valid & kFieldPm10) {
        PrintUnsignedTenth(sh, "PM10", m.pm10, "ug/m3");
    } else {
        PrintNa(sh, "PM10");
    }

    if (m.valid & kFieldHumidity) {
        PrintSignedFixed(sh, "Humidity", m.humidity, 100, "%RH");
    } else {
        PrintNa(sh, "Humidity");
    }
    if (m.valid & kFieldTemperature) {
        PrintSignedFixed(sh, "Temperature", static_cast<int32_t>(m.temperature) * 100 / 200, 100,
                         "degC");
    } else {
        PrintNa(sh, "Temperature");
    }
    if (m.valid & kFieldVocIndex) {
        PrintSignedFixed(sh, "VOC index", m.voc_index, 10, "");
    } else {
        PrintNa(sh, "VOC index");
    }
    if (m.valid & kFieldNoxIndex) {
        PrintSignedFixed(sh, "NOx index", m.nox_index, 10, "");
    } else {
        PrintNa(sh, "NOx index");
    }

    if (m.valid & kFieldCo2) {
        shell_print(sh, "  %-12s %u ppm", "CO2", static_cast<unsigned>(m.co2));
    } else {
        PrintNa(sh, "CO2");
    }
}

int CmdSerial(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    char serial[Sen66::kSerialChars];
    int err = Sen66::ReadSerialNumber(serial);
    if (err) {
        shell_error(sh, "SEN66: serial read failed (%d)", err);
        return err;
    }
    shell_print(sh, "SEN66 serial: %s", serial);
    return 0;
}

int CmdRead(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    // Idempotent for the smoke test: if a measurement is already running the
    // sensor NACKs the start, which we ignore before polling for data.
    Sen66::StartMeasurement();

    bool ready = false;
    for (int i = 0; i < kDataReadyPollCount && !ready; i++) {
        ready = Sen66::IsDataReady();
        if (!ready) {
            k_msleep(kDataReadyPollMs);
        }
    }
    if (!ready) {
        shell_warn(sh, "SEN66: no data ready (still warming up?)");
        return -EAGAIN;
    }

    Sen66::Measurement m;
    int err = Sen66::ReadMeasurement(m);
    if (err) {
        shell_error(sh, "SEN66: read failed (%d)", err);
        return err;
    }

    shell_print(sh, "SEN66 measurement:");
    PrintMeasurement(sh, m);
    return 0;
}

#if defined(CONFIG_APP_MEASUREMENT_SERVICE)
int CmdLatest(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    // Non-blocking: reads the measurement service's mutex-guarded snapshot
    // instead of issuing a fresh I2C transaction, so this also reflects
    // in-progress backoff/recovery after an I2C error or a disconnected
    // sensor.
    MeasurementService::Snapshot snapshot;
    if (!MeasurementService::GetLatest(snapshot)) {
        shell_warn(sh, "SEN66: no measurement yet (still starting up?)");
        return -EAGAIN;
    }

    shell_print(sh, "SEN66 latest (age %lld ms):", static_cast<long long>(snapshot.ageMs));
    PrintMeasurement(sh, snapshot.measurement);
    return 0;
}
#endif

} // namespace

#if defined(CONFIG_APP_MEASUREMENT_SERVICE)
SHELL_STATIC_SUBCMD_SET_CREATE(sen66_cmds,
                               SHELL_CMD(serial, NULL, "Read the SEN66 serial number", CmdSerial),
                               SHELL_CMD(read, NULL, "Read one SEN66 measurement", CmdRead),
                               SHELL_CMD(latest, NULL,
                                         "Print the measurement service's latest snapshot "
                                         "(non-blocking)",
                                         CmdLatest),
                               SHELL_SUBCMD_SET_END);
#else
SHELL_STATIC_SUBCMD_SET_CREATE(sen66_cmds,
                               SHELL_CMD(serial, NULL, "Read the SEN66 serial number", CmdSerial),
                               SHELL_CMD(read, NULL, "Read one SEN66 measurement", CmdRead),
                               SHELL_SUBCMD_SET_END);
#endif

SHELL_CMD_REGISTER(sen66, &sen66_cmds, "SEN66 air-quality sensor diagnostics", NULL);
