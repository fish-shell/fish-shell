# Fishscript tests
#
# There is no shebang line because you shouldn't be running this by hand. You
# should be running it via `make test` to ensure the environment is properly
# setup.

# Set this var to modify behavior of the code being tests. Such as avoiding running
# `fish_update_completions` when running tests.
set -x FISH_UNIT_TESTS_RUNNING 1

# Change to directory containing this script
cd (dirname (status -f))

# Test files specified on commandline, or all *.in files
if set -q argv[1]
    set files_to_test $argv.in
else
    set files_to_test *.in *.check
end

# These env vars should not be inherited from the user environment because they can affect the
# behavior of the tests. So either remove them or set them to a known value.
# See also tests/interactive.fish.
set TERM xterm
set -e ITERM_PROFILE

source test_util.fish (status -f) $argv
or exit

say -o cyan "Testing high level script functionality"

function test_in_file
    set -l file $argv[1]
    set -l base (basename $file .in)

    echo -n "Testing file $file ... "
    set starttime (date +%s)

    ../test/root/bin/fish <$file >$base.tmp.out 2>$base.tmp.err
    set -l exit_status $status
    set -l res ok
    set test_duration (math (date +%s) - $starttime)

    diff $base.tmp.out $base.out >/dev/null
    set -l out_status $status
    diff $base.tmp.err $base.err >/dev/null
    set -l err_status $status

    if test $out_status -eq 0 -a $err_status -eq 0 -a $exit_status -eq 0
        say green "ok ($test_duration sec)"
        # clean up tmp files
        rm -f $base.tmp.{err,out}
        return 0
    else
        say red "fail"
        if test $out_status -ne 0
            say yellow "Output differs for file $file. Diff follows:"
            colordiff -u $base.out $base.tmp.out
        end
        if test $err_status -ne 0
            say yellow "Error output differs for file $file. Diff follows:"
            colordiff -u $base.err $base.tmp.err
        end
        if test $exit_status -ne 0
            say yellow "Exit status differs for file $file."
            echo "Unexpected test exit status $exit_status."
        end
        return 1
    end
end

function test_littlecheck_file
    set -l file $argv[1]
    echo "Testing file $file"
    ../littlecheck.py -s fish=../test/root/bin/fish $file
end

set -l failed
for i in $files_to_test
    if string match --quiet '*.check' $i
        if not test_littlecheck_file $i
            # littlecheck test
            set failed $failed $i
        end
    else
        # .in test
        if not test_in_file $i
            set failed $failed $i
        end
    end
end

set failed (count $failed)
if test $failed -eq 0
    say green "All tests completed successfully"
    exit 0
else
    set plural (test $failed -eq 1; or echo s)
    say red "$failed test$plural failed"
    exit 1
end
