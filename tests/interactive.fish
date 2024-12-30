#! /bin/echo "interactive.fish must be run via the test driver!"
#
# Interactive tests using `pexpect`

# Set this var to modify behavior of the code being tests. Such as avoiding running
# `fish_update_completions` when running tests.
set -gx FISH_UNIT_TESTS_RUNNING 1

# Save the directory containing this script
# Do not *cd* here, otherwise you'll ruin our nice tmpdir setup!!!
set -l scriptdir (status dirname)

# Test files specified on commandline, or all pexpect files.
if set -q argv[1] && test -n "$argv[1]"
    set pexpect_files_to_test $scriptdir/pexpects/$argv
else if set -q FISH_PEXPECT_FILES
    set pexpect_files_to_test (string replace -r '^.*/(?=pexpects/)' '' -- $FISH_PEXPECT_FILES)
else
    say -o cyan "Testing interactive functionality"
    set pexpect_files_to_test $scriptdir/pexpects/*.py
end

source $scriptdir/test_util.fish || exit
cat $scriptdir/interactive.config >>$XDG_CONFIG_HOME/fish/config.fish
set -lx --prepend PYTHONPATH (realpath $scriptdir)

function test_pexpect_file
    set -l file $argv[1]
    echo -n "Testing file $file:"

    begin
        set starttime (timestamp)
        set -lx TERM dumb

        # Help the script find the pexpect_helper module in our parent directory.
        set -q FISHDIR
        or set -l FISHDIR ../test/root/bin/
        set -lx fish $FISHDIR/fish
        set -lx fish_key_reader $FISHDIR/fish_key_reader
        path is -fx -- $FISHDIR/fish_test_helper
        and set -lx fish_test_helper $FISHDIR/fish_test_helper

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

# The test here looks wrong, but False sets exit status to 0, which is what we want
if python3 -c 'import sys; exit(sys.version_info > (3, 5))'
    say red "pexpect tests disabled: python3 is too old"
    set pexpect_files_to_test
end
if not python3 -c 'import pexpect'
    say red "pexpect tests disabled: `python3 -c 'import pexpect'` failed"
    set pexpect_files_to_test
end
for i in $pexpect_files_to_test
    if not test_pexpect_file $i
        # Retry pexpect tests under CI twice, as they are timing-sensitive and CI resource
        # contention can cause tests to spuriously fail.
        if set -qx CI
            say yellow "Trying $i for a second time"
            if not test_pexpect_file $i
                set failed $failed $i
            end
        else
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
