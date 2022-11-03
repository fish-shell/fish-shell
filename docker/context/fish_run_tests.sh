#!/bin/bash

# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# This script is copied into the root directory of our Docker tests.
# It is the entry point for running Docker-based tests.

cd ~/fish-build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug /fish-source

# Spawn a shell if FISH_RUN_SHELL_BEFORE_TESTS is set.
if test -n "$FISH_RUN_SHELL_BEFORE_TESTS"
then
    bash -i || exit
fi

ninja && ninja fish_run_tests
RES=$?

# Drop the user into a shell if FISH_RUN_SHELL_AFTER_TESTS is set.
if test -n "$FISH_RUN_SHELL_AFTER_TESTS"; then
    bash -i
fi

exit $RES
