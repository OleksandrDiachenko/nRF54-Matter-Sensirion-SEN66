/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_task.h"

#include "app/matter_init.h"
#include "app/task_executor.h"
#include "board/board.h"
#include "lib/core/CHIPError.h"
#include "lib/support/CodeUtils.h"

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
