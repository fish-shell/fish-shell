#RUN: %fish -C 'set -l fish %fish' %s
# Test ALL THE FISH FILES
# in share/, that is - the tests are exempt because they contain syntax errors, on purpose

for file in $__fish_data_dir/**.fish
    $fish -n $file
end
# No output is good output
