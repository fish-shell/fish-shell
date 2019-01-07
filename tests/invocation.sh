#!/bin/sh
##
# Test that the invocation of the fish executable works as we hope.
#
# We try to run the 'fish' binary with different command line switches.
# Each time we check against an output that we expect.
#
# We are testing fish's invocation itself, so this is not written in
# fish itself - if the invocation wasn't working, we'd never even
# be able to use this test to check that the invocation wasn't working.
#
# What we test...
#
# * The environment is cleaned so that (hopefully) differences in
#   the host terminal, language or user settings do not affect the
#   tests.
#
# * The files 'tests/invocation/*.invoke' contain the arguments that
#   will be passed to the 'fish' command under test. The arguments
#   may be split over multiple lines for clarity.
#
# * Before execution, if the file 'tests/invocation/<name>.config'
#   exists, it will be copied as the 'config.fish' file in the
#   configuration directory.
#
# * The stdout and stderr are captured into files and will be
#   processed before comparison with the
#   'tests/invocation/<name>.(out|err)' files. A missing file is
#   considered to be no output.
#   Either file may be given a further suffix of '.<system name>'
#   which will be used in preference to the default. This allows
#   the expected output to change depending on the system being
#   used - to allow for differences in behaviour.
#   The '<system name>' can be found with 'uname -s'.
#   This facility should be used sparingly as system differences
#   will confuse users.
#
# * The file 'tests/invocation/<name>.grep' is used to select the
#   sections of the file we are interested in within the stdout.
#   Only the parts that match will be compared to the '*.out' file.
#   This can be used to filter out changeable parts of the output
#   leaving just the parts we are interested in.
#
# * The stderr output will have the 'RC: <return code>' appended
#   if the command returned a non-zero value.
#   The stderr output will have the 'XDG_CONFIG_HOME' location
#   substituted, to allow error reports to be compared consistently.
#
# * If the processed output differs from the supplied output,
#   the test will fail, and the differences will be shown on the
#   console.
#
# * If anything fails, the return code for this script will be
#   non-zero.
#

# Errors will be fatal
set -e

# The directory this script is in (as everything is relative to here)
here="$(cd "$(dirname "$0")" && pwd -P)"
cd "$here"

# The temporary directory to use
temp_dir="$here/../test"

# The fish binary we are testing - for manual testing, may be overridden
fish_exe="${fish_exe:-../test/root/bin/fish}"
fish_dir="$(dirname "${fish_exe}")"
fish_leaf="$(basename "${fish_exe}")"


# Which system are we on.
# fish has slightly different behaviour depending on the system it is
# running on (and the libraries that it is linked with), so for special
# cases, we'll use a suffixed file.
system_name="$(uname -s)"


# Check whether we have the 'colordiff' tool - if not, we'll revert to
# boring regular 'diff'.
if command -v colordiff >/dev/null 2>&1; then
    difftool='colordiff'
else
    difftool='diff'
fi


##
# Set variables to known values so that they will not affect the
# execution of the test.
clean_environment() {

    # Reset the terminal variables to a known type.
    export TERM=xterm
    unset ITERM_PROFILE

    # And the language as well, so that we do not see differences in
    # output dur to the user's locale
    export LANGUAGE=en_US:en

    # Ensure that the fish environment we use is in a clean state
    rm -rf "${temp_dir}/data" "${temp_dir}/home"
    mkdir -p "${temp_dir}/data" "${temp_dir}/home" "${temp_dir}/home/fish"
    export XDG_DATA_HOME="${temp_dir}/data"
    export XDG_CONFIG_HOME="${temp_dir}/home"
}


##
# Fail completely :-(
fail() {
    say "$term_red" "FAIL: $*" >&2
    exit 1
}


##
# Coloured output
#
# Use like `say "$term_green" "message".
say() {
    echo "$1$2$term_reset"
}

run_rc() {
    # Write the return code on to the end of the stderr, so that it can be
    # checked like anything else.
    eval "$*" || echo "RC: $?" >&2
}

filter() {
    # In some cases we want to check only a part of the output.
    # For those we filter the output through grep'd matches.
    if [ -f "$1" ] ; then
        # grep '-o', '-E' and '-f' are supported by the tools in modern GNU
        # environments, and on OS X.
        grep -oE -f "$1"
    else
        cat
    fi
}

##
# Actual testing of a .invoke file.
test_file() {
    local file="$1"
    local dir="$(dirname "$file")"
    local base="$(basename "$file" .invoke)"
    local test_config="${dir}/${base}.config"
    local test_stdout="${dir}/${base}.tmp.out"
    local test_stderr="${dir}/${base}.tmp.err"
    local want_stdout="${dir}/${base}.out"
    local grep_stdout="${dir}/${base}.grep"
    local want_stderr="${dir}/${base}.err"
    local empty="${dir}/${base}.empty"
    local filter
    local rc=0
    local test_args_literal
    local test_args
    local out_status=0
    local err_status=0

    # Literal arguments, for printing
    test_args_literal="$(cat "$file")"
    # Read the test arguments, escaping things that might be processed by us
    test_args="$(sed 's/\$/\$/' "$file" | tr '\n' ' ')"

    # Select system-specific files if they are present.
    system_specific=
    if [ -f "${test_config}.${system_name}" ] ; then
        test_config="${test_config}.${system_name}"
        system_specific=true
    fi
    if [ -f "${want_stdout}.${system_name}" ] ; then
        want_stdout="${want_stdout}.${system_name}"
        system_specific=true
    fi
    if [ -f "${want_stderr}.${system_name}" ] ; then
        want_stderr="${want_stderr}.${system_name}"
        system_specific=true
    fi
    if [ -f "${grep_stdout}.${system_name}" ] ; then
        grep_stdout="${grep_stdout}.${system_name}"
        system_specific=true
    fi

    # Create an empty file so that we can compare against it if needed
    echo -n > "${empty}"

    # If they supplied a configuration file, we create it here
    if [ -f "$test_config" ] ; then
        cat "$test_config" > "${temp_dir}/home/fish/config.fish"
    else
        rm -f "${temp_dir}/home/fish/config.fish"
    fi

    echo -n "Testing file $file ${system_specific:+($system_name specific) }... "

    # The hoops we are jumping through here, with changing directory are
    # so that we always execute fish as './fish', which means that any
    # error messages will appear the same, even if the tested binary
    # is not one that we built here.
    # We disable the exit-on-error here, so that we can catch the return
    # code.
    set +e
    run_rc "cd \"$fish_dir\" && \"./$fish_leaf\" $test_args" \
           2> "$test_stderr" \
           < /dev/null       \
           | filter "$grep_stdout" \
           > "$test_stdout"
    set -e

    # If the wanted output files are not present, they are assumed empty.
    if [ ! -f "$want_stdout" ] ; then
        want_stdout="$empty"
    fi
    if [ ! -f "$want_stderr" ] ; then
        want_stderr="$empty"
    fi

    # The standard error that we get will report errors using non-relative
    # filenames, so we try to replace these with the variable names.
    #
    # However, fish will also have helpfully translated the home directory
    # into '~/' in the error report. Consequently, we need to perform a
    # small fix-up so that we can replace the string sanely.
    xdg_config_in_home="${XDG_CONFIG_HOME#$HOME}"
    if [ "${#xdg_config_in_home}" -lt "${#XDG_CONFIG_HOME}" ]; then
        xdg_config_in_home="~$xdg_config_in_home"
    fi
    # 'sed -i' (inplace) has different syntax on BSD and GNU versions of
    # the tool, so cannot be used here, hence we write to a separate file,
    # and then move back.
    sed "s,$xdg_config_in_home,\$XDG_CONFIG_HOME,g" "${test_stderr}" > "${test_stderr}.new"
    mv -f "${test_stderr}.new" "${test_stderr}"

    # Check the results
    if ! diff "${test_stdout}" "${want_stdout}" >/dev/null 2>/dev/null ; then
        out_status=1
    fi
    if ! diff "${test_stderr}" "${want_stderr}" >/dev/null 2>/dev/null ; then
        err_status=1
    fi

    if [ "$out_status" = '0' ] && \
       [ "$err_status" = '0' ] ; then
        say "$term_green" "ok"
        # clean up tmp files
        rm -f "${test_stdout}" "${test_stderr}" "${empty}"
        rc=0
    else
        say "$term_red" "fail"
        say "$term_blue" "$test_args_literal" | sed 's/^/    /'

        if [ "$out_status" != '0' ] ; then
            say "$term_yellow" "Output differs for file $file. Diff follows:"
            "$difftool" -u "${test_stdout}" "${want_stdout}"
        fi
        if [ "$err_status" != '0' ] ; then
            say "$term_yellow" "Error output differs for file $file. Diff follows:"
            "$difftool" -u "${test_stderr}" "${want_stderr}"
        fi
        rc=1
    fi

    return $rc
}


########################################################################
# Main harness

if [ ! -x "${fish_exe}" ] ; then
    fail "Fish executable not found at '${fish_exe}'"
fi

clean_environment

# Terminal colouring
# Only do this after setting up $TERM.
term_red=""
term_green=""
term_yellow=""
term_blue=""
term_magenta=""
term_cyan=""
term_white=""
term_reset=""
# Some systems don't have tput. Disable coloring.
if command -v tput >/dev/null 2>&1; then
    term_red="$(tput setaf 1)"
    term_green="$(tput setaf 2)"
    term_yellow="$(tput setaf 3)"
    term_blue="$(tput setaf 4)"
    term_magenta="$(tput setaf 5)"
    term_cyan="$(tput setaf 6)"
    term_white="$(tput setaf 7)"
    term_reset="$(tput sgr0)"
fi

say "$term_cyan" "Testing shell invocation functionality"

passed=0
failed=0
for file in invocation/*.invoke; do
   if ! test_file "$file" ; then
       failed=$(( failed + 1 ))
   else
       passed=$(( passed + 1 ))
   fi
done

echo "Encountered $failed errors in the invocation tests (out of $(( failed + passed )))."

if [ "$failed" != 0 ] ; then
    exit 1
fi
exit 0
