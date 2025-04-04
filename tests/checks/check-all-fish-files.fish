#RUN: fish=%fish %fish %s
# disable on CI ASAN because it's suuuper slow
#REQUIRES: test -z "$FISH_CI_SAN"
# Test ALL THE FISH FILES
# in share/, that is - the tests are exempt because they contain syntax errors, on purpose

set -l dir (path resolve -- (status dirname)/../../)
set timestamp_file ./last_check_all_files
set -l find_args
if test -f $timestamp_file
    set find_args -newer $timestamp_file
end
set -l fail_count 0
for file in (find $dir -name "*.fish" $find_args 2>/dev/null; or find $dir -name "*.fish")
    $fish -n $file; or set fail_count (math $fail_count + 1)
end

# Prevent setting timestamp if any errors were encountered
if test "$fail_count" -eq 0
    touch $timestamp_file
end

# No output is good output
