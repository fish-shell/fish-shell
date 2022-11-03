# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# Support for benchmarking fish.

add_custom_target(benchmark
    COMMAND ${CMAKE_SOURCE_DIR}/benchmarks/driver.sh $<TARGET_FILE:fish>
    USES_TERMINAL
)
