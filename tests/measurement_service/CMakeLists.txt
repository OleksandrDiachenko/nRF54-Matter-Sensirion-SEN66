# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(measurement_service_unit)

# The policy is pure (no Zephyr dependency, only stdint/stddef plus the
# sen66_protocol.h struct/enum declarations), so it compiles straight into
# the host test binary alongside the ztest sources - no I2C driver needed.
set(APP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(MEASUREMENT_SRC ${APP_SRC}/measurement_service)

target_include_directories(testbinary PRIVATE ${APP_SRC} ${MEASUREMENT_SRC})

target_sources(testbinary PRIVATE
    ${MEASUREMENT_SRC}/measurement_policy.cpp
    src/test_policy.cpp
)
