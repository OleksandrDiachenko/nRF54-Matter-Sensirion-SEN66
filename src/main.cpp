/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_task.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_CHIP_APP_LOG_LEVEL);

int main() {
    const CHIP_ERROR error = AppTask::Instance().StartApp();

    LOG_ERR("Matter application exited: %" CHIP_ERROR_FORMAT, error.Format());
    return error == CHIP_NO_ERROR ? 0 : 1;
}
