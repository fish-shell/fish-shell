#! /bin/echo "interactive.fish must be run via the test driver!"
#
# Interactive tests using `pexpect`

# Set this var to modify behavior of the code being tests. Such as avoiding running
# `fish_update_completions` when running tests.
set -gx FISH_UNIT_TESTS_RUNNING 1

# Change to directory containing this script
cd (status dirname)

# Test files specified on commandline, or all pexpect files.
if set -q argv[1] && test -n "$argv[1]"
    set pexpect_files_to_test pexpects/$argv
else if set -q FISH_PEXPECT_FILES
    set pexpect_files_to_test (string replace -r '^.*/(?=pexpects/)' '' -- $FISH_PEXPECT_FILES)
else
    say -o cyan "Testing interactive functionality"
    set pexpect_files_to_test pexpects/*.py
end

source test_util.fish || exit
cat interactive.config >>$XDG_CONFIG_HOME/fish/config.fish

function test_pexpect_file
    set -l file $argv[1]
    echo -n "Testing file $file:"

    begin
        set starttime (timestamp)
        set -lx TERM dumb

        # Help the script find the pexpect_helper module in our parent directory.
        set -lx --prepend PYTHONPATH (realpath $PWD/..)
        set -lx fish ../test/root/bin/fish
        set -lx fish_key_reader ../test/root/bin/fish_key_reader
        set -lx fish_test_helper ../test/root/bin/fish_test_helper

        # Note we require Python3.
        python3 $file
    end

    set -l exit_status $status
    if test "$exit_status" -eq 0
        set test_duration (delta $starttime)
        say green "ok ($test_duration $unit)"
    else if test "$exit_status" -eq 127
        say blue "SKIPPED"
        set exit_status 0
    end
    return $exit_status
end

set failed

if not python3 -c 'import pexpect'
    say red "pexpect tests disabled: `python3 -c 'import pexpect'` failed"
    set pexpect_files_to_test
end
for i in $pexpect_files_to_test
    if not test_pexpect_file $i
        say yellow "Trying $i for a second time"
        if not test_pexpect_file $i
            set failed $failed $i
        end
    end
end

set failed (count $failed)
if test $failed -eq 0
    if test (count $pexpect_files_to_test) -gt 1
        say green "All interactive tests completed successfully"
    else
        say green "$pexpect_files_to_test completed successfully"
    end
    exit 0
else
    set plural (test $failed -eq 1; or echo s)
    say red "$failed test$plural failed"
    exit 1
end
