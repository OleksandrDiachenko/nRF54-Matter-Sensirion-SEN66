# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sen66_unit)

# The parser and CRC are pure (no Zephyr dependency), so they compile straight
# into the host test binary alongside the ztest sources.
set(SEN66_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/sen66)

target_include_directories(testbinary PRIVATE ${SEN66_SRC})

target_sources(testbinary PRIVATE
    ${SEN66_SRC}/sen66_crc.cpp
    ${SEN66_SRC}/sen66_protocol.cpp
    src/test_crc.cpp
    src/test_parser.cpp
)
