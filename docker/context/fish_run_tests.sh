#!/bin/sh

set -e

# This script is copied into the root directory of our Docker tests.
# It is the entry point for running Docker-based tests.

echo build_tools/check.sh >>~/.bash_history

cd /fish-source
git config --global --add safe.directory /fish-source

export CARGO_TARGET_DIR="$HOME"/fish-build

interactive_shell() {
    echo
    echo "+ export=CARGO_TARGET_DIR=$CARGO_TARGET_DIR"
    echo
    bash -i
}

# Spawn a shell if FISH_RUN_SHELL_BEFORE_TESTS is set.
if test -n "$FISH_RUN_SHELL_BEFORE_TESTS"
then
    interactive_shell
fi

set +e
build_tools/check.sh
RES=$?
set -e

# Drop the user into a shell if FISH_RUN_SHELL_AFTER_TESTS is set.
if test -n "$FISH_RUN_SHELL_AFTER_TESTS"; then
    interactive_shell
fi

exit $RES
