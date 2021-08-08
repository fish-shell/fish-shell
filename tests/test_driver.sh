# vim: set ts=4 sw=4 tw=100 et:
# POSIX sh test driver to reduce dependency on fish in tests

# macOS has really weird default IFS behavior that splits output in random places, and the trailing
# backspace is to prevent \n from being gobbled up by the subshell output substitution.
# Folks, this is why you should use fish!
IFS="$(printf "\n\b")"

# The first argument is the path to the script to launch; all remaining arguments are forwarded to
# the script.
fish_script="$1"
shift 1
script_args="${@}"

die() {
    if test "$#" -ge 0; then
        printf "%s\n" "$@" 1>&2
    fi
    exit 1
}

# To keep things sane and to make error messages comprehensible, do not use relative paths anywhere
# in this script. Instead, make all paths relative to one of these or $homedir."
TESTS_ROOT="$(dirname "$0")"
BUILD_ROOT="${TESTS_ROOT}/.."

# macOS (still!) doesn't have `readlink -f` or `realpath`. That's OK, this is just for aesthetics.
if command -v realpath 1>/dev/null 2>/dev/null; then
    TESTS_ROOT="$(realpath --no-symlinks "${TESTS_ROOT}")"
    BUILD_ROOT="$(realpath --no-symlinks "${BUILD_ROOT}")"
fi

if ! test -z "$__fish_is_running_tests"; then
    echo "Recursive test invocation detected!" 1>&2
    exit 10
fi

# Set up a test environment and re-run the original script. We do not share environments
# whatsoever between tests, so each test driver run sets up a new profile altogether.

homedir="$(mktemp -d)"

# cp -a ../test/xdg_data_home "$homedir/"
XDG_DATA_HOME="$homedir/xdg_data_home"
export XDG_DATA_HOME
mkdir -p $XDG_DATA_HOME/fish || die

# cp -a ../test/xdg_config_home "$homedir/"
XDG_CONFIG_HOME="$homedir/xdg_config_home"
export XDG_CONFIG_HOME
mkdir -p $XDG_CONFIG_HOME/fish || die

XDG_RUNTIME_DIR="$homedir/xdg_runtime_dir"
export XDG_CONFIG_HOME
mkdir -p $XDG_RUNTIME_DIR/fish || die

# These are used read-only so it's OK to symlink instead of copy
rm -f "$XDG_CONFIG_HOME/fish/functions"
ln -s "$PWD/test_functions" "$XDG_CONFIG_HOME/fish/functions" || die "Failed to symlink"

# Set the function path at startup, referencing the default fish functions and the test-specific
# functions.
fish_init_cmd="set fish_function_path ${XDG_CONFIG_HOME}/fish/functions ${BUILD_ROOT}/share/functions"

__fish_is_running_tests="$homedir"
export __fish_is_running_tests

# Set locale information for consistent tests. Fish should work with a lot of locales but the
# tests assume an english UTF-8 locale unless they explicitly override this default. We do not
# want the users locale to affect the tests since they might, for example, change the wording of
# logged messages.
#
# TODO: set LANG to en_US.UTF-8 so we test the locale message conversions (i.e., gettext).
unset LANGUAGE
# Remove "LC_" env vars from the test environment
for key in $(env | grep -E "^LC_"| grep -oE "^[^=]+"); do
    unset "$key"
done
# Set the desired lang/locale tests are hard-coded against
export LANG="C"
export LC_CTYPE="en_US.UTF-8"

# These env vars should not be inherited from the user environment because they can affect the
# behavior of the tests. So either remove them or set them to a known value.
# See also tests/interactive.fish.
export TERM=xterm
unset COLORTERM
unset INSIDE_EMACS
unset ITERM_PROFILE
unset KONSOLE_PROFILE_NAME
unset KONSOLE_VERSION
unset PANTHEON_TERMINAL_ID
unset TERM_PROGRAM
unset TERM_PROGRAM_VERSION
unset VTE_VERSION
unset WT_PROFILE_ID
unset XTERM_VERSION

# Set a marker to indicate whether colored output should be suppressed (used in `test_util.fish`)
suppress_color=""
if ! tty 0>&1 > /dev/null; then
    suppress_color="yes"
fi
export suppress_color

# Source test util functions at startup
fish_init_cmd="${fish_init_cmd} && source ${TESTS_ROOT}/test_util.fish";

# Run the test script, but don't exec so we can do cleanup after it succeeds/fails
# echo $PWD
# echo "BUILD_ROOT: $BUILD_ROOT"
# echo "TESTS_ROOT: $TESTS_ROOT"
env HOME=$homedir "${BUILD_ROOT}/test/root/bin/fish" \
    --init-command "${fish_init_cmd}" \
    "$fish_script" "$script_args"
test_status="$?"

echo test completed in $homedir with status $test_status

rm -rf "$homedir"
exit "$test_status"
