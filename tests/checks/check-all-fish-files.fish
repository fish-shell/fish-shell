#RUN: fish=%fish %fish %s
# disable on CI ASAN because it's suuuper slow
#REQUIRES: test -z "$FISH_CI_SAN"

set -l workspace_root (path resolve -- (status dirname)/../../)
set timestamp_file $workspace_root/tests/.last-check-all-files
set -l find_args
if test -f $timestamp_file
    set find_args -newer $timestamp_file
end
set -l fail_count 0
for file in (find $workspace_root/{benchmarks,build_tools,etc,share,tests} -name "*.fish" $find_args)
    $fish -n $file; or set fail_count (math $fail_count + 1)
end

# Prevent setting timestamp if any errors were encountered
if test "$fail_count" -eq 0
    touch $timestamp_file 2>/dev/null
end

# No output is good output
