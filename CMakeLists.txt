cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})

project(nrf54_matter_sensirion_sen66)

include(${ZEPHYR_CONNECTEDHOMEIP_MODULE_DIR}/config/nrfconnect/app/enable-gnu-std.cmake)
include(${ZEPHYR_NRF_MODULE_DIR}/samples/matter/common/cmake/source_common.cmake)
include(${ZEPHYR_NRF_MODULE_DIR}/samples/matter/common/cmake/data_model.cmake)
include(${ZEPHYR_NRF_MODULE_DIR}/samples/matter/common/cmake/zap_helpers.cmake)

ncs_get_zap_parent_dir(ZAP_PARENT_DIR)

target_include_directories(app PRIVATE
    src
    ${ZAP_PARENT_DIR}
)

target_sources(app PRIVATE
    src/app_task.cpp
    src/main.cpp
)

if(CONFIG_APP_SEN66)
    target_sources(app PRIVATE
        src/sen66/sen66_crc.cpp
        src/sen66/sen66_protocol.cpp
        src/sen66/sen66_driver.cpp
    )
endif()

if(CONFIG_APP_MEASUREMENT_SERVICE)
    target_sources(app PRIVATE
        src/measurement_service/measurement_policy.cpp
        src/measurement_service/measurement_service.cpp
    )
endif()

if(CONFIG_APP_SEN66_SHELL)
    target_sources(app PRIVATE
        src/sen66/sen66_shell.cpp
    )
endif()

ncs_configure_data_model()
