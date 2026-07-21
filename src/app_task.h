/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include <platform/CHIPDeviceLayer.h>

class AppTask {
  public:
    static AppTask &Instance() {
        static AppTask instance;
        return instance;
    }

    CHIP_ERROR StartApp();

  private:
    CHIP_ERROR Init();
};
