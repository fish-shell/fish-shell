#RUN: %fish -C 'set -l fish %fish' %s
# Test ALL THE FISH FILES
# in share/, that is - the tests are exempt because they contain syntax errors, on purpose

set timestamp_file ./last_check_all_files
set -l find_args
if test -f $timestamp_file
    set find_args -mnewer $timestamp_file
end
for file in (find $__fish_data_dir/ -name "*.fish" $find_args)
    $fish -n $file
end
touch $timestamp_file
# No output is good output
