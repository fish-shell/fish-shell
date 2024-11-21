# Fishscript tests
#
# There is no shebang line because you shouldn't be running this by hand. You
# should be running it via `make test` to ensure the environment is properly
# setup.

# Set this var to modify behavior of the code being tests. Such as avoiding running
# `fish_update_completions` when running tests.
set -x FISH_UNIT_TESTS_RUNNING 1

# Change to directory containing this script
cd (status dirname)

# Test files specified on commandline, or all checks.
set -l files_to_test
if set -q argv[1]
    set files_to_test checks/$argv.fish
else
    set files_to_test checks/*.fish
end

# Be less verbose when running tests one-by-one
if test (count $files_to_test) -gt 1
    say -o cyan "Testing high level script functionality"
end

set -g python (__fish_anypython)
or begin
    say red "Python is not installed. These tests require python."
    exit 125
end

# Test littlecheck files.
set -l skipped 0
set -l failed 0
if set -q files_to_test[1]
    set -l force_color
    test "$FISH_FORCE_COLOR" = 1
    and set force_color --force-color

    $python -S ../littlecheck.py \
        --progress $force_color \
        -s fish=../test/root/bin/fish \
        -s fish_test_helper=../test/root/bin/fish_test_helper \
        -s filter-control-sequences='../test/root/bin/fish ../tests/filter-control-sequences.fish' \
        $files_to_test

    set -l littlecheck_status $status
    if test "$littlecheck_status" -eq 125
        # 125 indicates that all tests executed were skipped.
        set skipped (count $files_to_test)
    else
        # The return code indicates the number of tests that failed
        set failed $littlecheck_status
    end
end

if test $failed -eq 0 && test $skipped -gt 0
    test (count $files_to_test) -gt 1 && say blue (count $files_to_test)" tests skipped"
    exit 125
else if test $failed -eq 0
    test (count $files_to_test) -gt 1 && say green "All high level script tests completed successfully"
    exit 0
else
    test (count $files_to_test) -gt 1 && say red "$failed tests failed"
    exit 1
end
