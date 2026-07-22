# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(air_quality_endpoint_unit)

# The policy is pure (no Zephyr dependency, only stdint plus the
# sen66_protocol.h struct/enum declarations), so it compiles straight into
# the host test binary - no Matter/CHIP stack needed.
set(APP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(AIR_QUALITY_SRC ${APP_SRC}/air_quality_endpoint)

target_include_directories(testbinary PRIVATE ${APP_SRC} ${AIR_QUALITY_SRC})

target_sources(testbinary PRIVATE
    ${AIR_QUALITY_SRC}/air_quality_policy.cpp
    src/test_policy.cpp
)
