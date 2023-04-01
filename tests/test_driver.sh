# vim: set ts=4 sw=4 tw=100 et:

# POSIX sh test driver to reduce dependency on ghoti in tests.
# Executes the specified *ghoti script* with the provided arguments, after setting up a clean test
# environment (see `test_env.sh`) and then importing the ghoti-related helper functions and
# performing some state initialization required by the various ghoti tests. Each payload script is
# executed in its own environment, this script waits for ghoti to exit then cleans up the target
# environment and bubbles up the ghoti exit code.

# macOS has really weird default IFS behavior that splits output in random places, and the trailing
# backspace is to prevent \n from being gobbled up by the subshell output substitution.
# Folks, this is why you should use ghoti!
IFS="$(printf "\n\b")"

# The first argument is the path to the script to launch; all remaining arguments are forwarded to
# the script.
ghoti_script="$1"
shift 1
script_args="${@}"
# Prevent $@ from persisting to sourced commands
shift $# 2>/dev/null

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

if ! test -z "$__ghoti_is_running_tests"; then
    echo "Recursive test invocation detected!" 1>&2
    exit 10
fi

# Set up the test environment. Does not change the current working directory.
. ${TESTS_ROOT}/test_env.sh

# These are used read-only so it's OK to symlink instead of copy
rm -f "$XDG_CONFIG_HOME/ghoti/functions"
ln -s "$PWD/test_functions" "$XDG_CONFIG_HOME/ghoti/functions" || die "Failed to symlink"

# Set the function path at startup, referencing the default ghoti functions and the test-specific
# functions.
ghoti_init_cmd="set ghoti_function_path '${XDG_CONFIG_HOME}/ghoti/functions' '${BUILD_ROOT}/share/functions'"

__ghoti_is_running_tests="$homedir"
export __ghoti_is_running_tests

# Set a marker to indicate whether colored output should be suppressed (used in `test_util.ghoti`)
suppress_color=""
if ! tty 0>&1 > /dev/null; then
    suppress_color="yes"
fi
export suppress_color

# Source test util functions at startup
ghoti_init_cmd="${ghoti_init_cmd} && source ${TESTS_ROOT}/test_util.ghoti";

# Run the test script, but don't exec so we can clean up after it succeeds/fails. Each test is
# launched directly within its TMPDIR, so that the ghoti tests themselves do not need to refer to
# TMPDIR (to ensure their output as displayed in case of failure by littlecheck is reproducible).
(cd $TMPDIR && env HOME="$homedir" "${BUILD_ROOT}/test/root/bin/ghoti" \
    --init-command "${ghoti_init_cmd}" "$ghoti_script" "$script_args")
test_status="$?"

# CMake less than 3.9.0 "fully supports" setting an exit code to denote a skipped test, but then
# it just goes ahead and reports them as failed anyway. Really?
if test -n $CMAKE_SKIPPED_HACK; then
    if test $test_status -eq 125; then
        echo "Overriding SKIPPED return code from test" 1>&2
        test_status=0
    fi
fi

rm -rf "$homedir"
exit "$test_status"
