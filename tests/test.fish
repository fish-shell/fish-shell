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

# test_util handles the environment setup and then restarts us
source test_util.fish (status -f) $argv
or exit

say -o cyan "Testing high level script functionality"

set -g python (__fish_anypython)

# Test littlecheck files.
set littlecheck_files (string match '*.fish' -- $files_to_test)
if set -q littlecheck_files[1]
    $python -S ../littlecheck.py \
        --progress \
        -s fish=../test/root/bin/fish \
        -s fish_test_helper=../test/root/bin/fish_test_helper \
        $littlecheck_files
    set -l littlecheck_failures $status
    set failed (math $failed + $littlecheck_failures)
end

if test $failed -eq 0
    say green "All high level script tests completed successfully"
    exit 0
else
    set plural (test $failed -eq 1; or echo s)
    say red "$failed test$plural failed"
    exit 1
end
