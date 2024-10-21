# vim: set ts=4 sw=4 tw=100 et:

# This script sets up a clean environment for a script or executable to execute in/under. It creates
# (and sets) $TMPDIR initialized to a clean & unique temporary directory, creates a new $HOME, sets
# the relevant XDG_* directories to point to subdirectories of that $HOME, cleans up any potentially
# problematic environment variables, executes the provided command, waits for it to exit, then
# cleans up the newly created environment before bubbling up the exit code. $PWD is not changed.
# If sourced instead of executed, sets up the environment as before but does not execute any payload
# and does not destroy the newly created environment.

# macOS has really weird default IFS behavior that splits output in random places, and the trailing
# backspace is to prevent \n from being gobbled up by the subshell output substitution.
# Folks, this is why you should use fish!
IFS="$(printf "\n\b")"
# set -ex

# The first argument is the path to the script to launch; all remaining arguments are forwarded to
# the script.
if test $# -gt 0; then
    target="$1"
    shift 1
    target_args="${@}"
fi

die() {
    if test "$#" -ge 0; then
        printf "%s\n" "$@" 1>&2
    fi
    exit 1
}

# Set up a test environment to run the specified target under. We do not share environments
# whatsoever between tests, so each test driver run sets up a new profile altogether.

# macOS 10.10 requires an explicit template for `mktemp` and will create the folder in the
# current directory unless told otherwise. Linux isn't guaranteed to have $TMPDIR set.
homedir="$(mktemp -d 2>/dev/null || mktemp -d "${TMPDIR}tmp.XXXXXXXXXX")"
export HOME="$homedir"

XDG_DATA_HOME="$homedir/xdg_data_home"
export XDG_DATA_HOME
mkdir -p $XDG_DATA_HOME/fish || die

XDG_CONFIG_HOME="$homedir/xdg_config_home"
export XDG_CONFIG_HOME
mkdir -p $XDG_CONFIG_HOME/fish || die

XDG_RUNTIME_DIR="$homedir/xdg_runtime_dir"
export XDG_RUNTIME_DIR
mkdir -p $XDG_RUNTIME_DIR/fish || die
chmod 700 "$XDG_RUNTIME_DIR"

XDG_CACHE_HOME="$homedir/xdg_cache_home"
export XDG_CACHE_HOME
mkdir -p $XDG_CACHE_HOME/fish || die

# Create a temp/scratch directory for tests to use, if they want (tests shouldn't write to a
# shared temp folder).
TMPDIR="$homedir/temp"
mkdir ${TMPDIR}
export TMPDIR

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
unset KONSOLE_PROFILE_NAME
unset KONSOLE_VERSION
unset PANTHEON_TERMINAL_ID
unset LC_TERMINAL
unset LC_TERMINAL_VERSION
unset TERM_PROGRAM
unset TERM_PROGRAM_VERSION
unset VTE_VERSION

# If we are sourced, return without executing
if test -z ${target}; then
    return 0
fi
echo "Proceeding with target execution"

# Otherwise execute target
("${target}" "${target_args}")
test_status="$?"

rm -rf "$homedir"
exit "$test_status"
