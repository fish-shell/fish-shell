# Interactive tests using `expect`
#
# There is no shebang line because you shouldn't be running this by hand. You
# should be running it via `make test` to ensure the environment is properly
# setup.

# Set this var to modify behavior of the code being tests. Such as avoiding running
# `fish_update_completions` when running tests.
set -gx FISH_UNIT_TESTS_RUNNING 1

# Change to directory containing this script
cd (status dirname)

# These env vars should not be inherited from the user environment because they can affect the
# behavior of the tests. So either remove them or set them to a known value.
# See also tests/test.fish.
set -gx TERM xterm
set -e ITERM_PROFILE

# Test files specified on commandline, or all pexpect files.
if set -q argv[1]
    set pexpect_files_to_test pexpects/$argv.py
else
    set pexpect_files_to_test pexpects/*.py
end

source test_util.fish (status -f) $argv
or exit
cat interactive.config >>$XDG_CONFIG_HOME/fish/config.fish

say -o cyan "Testing interactive functionality"
function test_pexpect_file
    set -l file $argv[1]
    echo -n "Testing file $file ... "

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
    say green "All interactive tests completed successfully"
    exit 0
else
    set plural (test $failed -eq 1; or echo s)
    say red "$failed test$plural failed"
    exit 1
end
