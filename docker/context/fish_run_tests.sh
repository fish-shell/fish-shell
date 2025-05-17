#!/bin/sh

set -e

# This script is copied into the root directory of our Docker tests.
# It is the entry point for running Docker-based tests.

cd ~/fish-build
git config --global --add safe.directory /fish-source
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug /fish-source "$@"

# Spawn a shell if FISH_RUN_SHELL_BEFORE_TESTS is set.
if test -n "$FISH_RUN_SHELL_BEFORE_TESTS"
then
    bash -i
fi

(set +e; ninja && ninja fish_run_tests)
RES=$?

# Drop the user into a shell if FISH_RUN_SHELL_AFTER_TESTS is set.
if test -n "$FISH_RUN_SHELL_AFTER_TESTS"; then
    bash -i
fi

exit $RES
