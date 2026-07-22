/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_task.h"

#include "app/matter_init.h"
#include "app/task_executor.h"
#include "board/board.h"
#include "lib/core/CHIPError.h"
#include "lib/support/CodeUtils.h"

#if defined(CONFIG_APP_SEN66)
#include "sen66/sen66_driver.h"
#endif

#if defined(CONFIG_APP_MEASUREMENT_SERVICE)
#include "measurement_service/measurement_service.h"
#endif

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

using namespace chip;

CHIP_ERROR AppTask::Init() {
    LOG_INF("Preparing Matter server");
    ReturnErrorOnFailure(Nrf::Matter::PrepareServer());
    LOG_INF("Matter server prepared");

    if (!Nrf::GetBoard().Init()) {
        LOG_ERR("Board UI initialization failed");
        return CHIP_ERROR_INCORRECT_STATE;
    }

#if defined(CONFIG_APP_SEN66)
    // A missing sensor bus must not block Matter. The measurement service
    // owns runtime polling, retry, and recovery once the bus is ready.
    if (!Sen66::Init()) {
        LOG_WRN("SEN66 sensor not detected; continuing without it");
    }
#if defined(CONFIG_APP_MEASUREMENT_SERVICE)
    else if (!MeasurementService::Init()) {
        LOG_ERR("SEN66 measurement service failed to start");
    }
#endif
#endif

    ReturnErrorOnFailure(
        Nrf::Matter::RegisterEventHandler(Nrf::Board::DefaultMatterEventHandler, 0));

    LOG_INF("Starting Matter server");
    return Nrf::Matter::StartServer();
}

CHIP_ERROR AppTask::StartApp() {
    ReturnErrorOnFailure(Init());

    while (true) {
        Nrf::DispatchNextTask();
    }
}
