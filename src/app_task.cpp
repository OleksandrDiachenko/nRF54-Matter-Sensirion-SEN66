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
    // Verify the sensor bus only. A missing sensor must not block Matter; polling
    // and measurement policy are added in the measurement service (M3).
    if (!Sen66::Init()) {
        LOG_WRN("SEN66 sensor not detected; continuing without it");
    }
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
