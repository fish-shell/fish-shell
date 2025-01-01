#!/bin/sh
# vim: set ts=4 sw=4 tw=100 et:

# POSIX sh test driver to reduce dependency on fish in tests.
# Executes the specified *fish script* with the provided arguments, after setting up a clean test
# environment (see `test_env.sh`) and then importing the fish-related helper functions and
# performing some state initialization required by the various fish tests. Each payload script is
# executed in its own environment, this script waits for fish to exit then cleans up the target
# environment and bubbles up the fish exit code.

# macOS has really weird default IFS behavior that splits output in random places, and the trailing
# backspace is to prevent \n from being gobbled up by the subshell output substitution.
# Folks, this is why you should use fish!
IFS="$(printf "\n\b")"

# If CDPATH is set, `cd foo` will print the directory.
unset CDPATH

# The first argument is the path to the script to launch; all remaining arguments are forwarded to
# the script.
# Resolve the script now because we are going to `cd` later.
fish_script="$(realpath $1)"
shift 1

die() {
    if test "$#" -ge 0; then
        printf "%s\n" "$@" 1>&2
    fi
    exit 1
}

# To keep things sane and to make error messages comprehensible, do not use relative paths anywhere
# in this script. Instead, make all paths relative to one of these or the new $HOME ($homedir)."
TESTS_ROOT="$(cd $(dirname "$0") && pwd -P)"
BUILD_ROOT="$(cd $(dirname "$TESTS_ROOT") && pwd -P)"

if test -z "$FISHDIR"; then
    die "Please set \$FISHDIR to a directory that contains fish, fish_indent and fish_key_reader"
fi

FISHDIR=$(realpath -- "$FISHDIR")
fish="${FISHDIR}/fish"

if ! test -x "$fish" || ! test -f "$fish"; then
    printf '%s\n' "'$fish' is not an executable fish." \
           "Please set \$FISHDIR to a directory that contains fish, fish_indent and fish_key_reader" >&2
    exit 7
fi

if ! test -z "$__fish_is_running_tests"; then
    echo "Recursive test invocation detected!" 1>&2
    exit 10
fi

# Set up the test environment. Does not change the current working directory.
. ${TESTS_ROOT}/test_env.sh

test -n "$homedir" || die "Failed to set up home"

# Compile our fish_test_helper program now.
# This takes about 50ms.
if command -v cc >/dev/null ; then
    cc "$TESTS_ROOT/fish_test_helper.c" -o "$homedir/fish_test_helper"
else
    echo "Cannot find a C compiler. Skipping tests that require fish_test_helper" >&2
fi

# These are used read-only so it's OK to symlink instead of copy
rm -f "$XDG_CONFIG_HOME/fish/functions"
ln -s "${TESTS_ROOT}/test_functions" "$XDG_CONFIG_HOME/fish/functions" || die "Failed to symlink"

# Set the function path at startup, referencing the default fish functions and the test-specific
# functions.
fish_init_cmd="set fish_function_path '${XDG_CONFIG_HOME}/fish/functions' '${BUILD_ROOT}/share/functions'"

__fish_is_running_tests="$homedir"
export __fish_is_running_tests

# Set a marker to indicate whether colored output should be suppressed (used in `test_util.fish`)
suppress_color=""
if ! tty 0>&1 > /dev/null; then
    suppress_color="yes"
fi
export suppress_color

# Source test util functions at startup
fish_init_cmd="${fish_init_cmd} && source ${TESTS_ROOT}/test_util.fish";

# Indicate that the fish panic handler shouldn't wait for input to prevent tests from hanging
FISH_FAST_FAIL=1
export FISH_FAST_FAIL

# Run the test script, but don't exec so we can clean up after it succeeds/fails. Each test is
# launched directly within its TMPDIR, so that the fish tests themselves do not need to refer to
# TMPDIR (to ensure their output as displayed in case of failure by littlecheck is reproducible).
if test -n "${@}"; then
    (cd $TMPDIR && env HOME="$homedir" fish_test_helper="$homedir/fish_test_helper" "$fish" \
                       --init-command "${fish_init_cmd}" "$fish_script" "${@}")
else
    (cd $TMPDIR && env HOME="$homedir" fish_test_helper="$homedir/fish_test_helper" "$fish" \
                       --init-command "${fish_init_cmd}" "$fish_script")
fi
test_status="$?"

rm -rf "$homedir"
exit "$test_status"
