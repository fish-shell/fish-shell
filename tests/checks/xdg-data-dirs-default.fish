#RUN: fish=%fish %fish %s

set -e FISH_UNIT_TESTS_RUNNING
function expect-datadirs
    $fish -c '
        for datadir in $argv
            contains -- $datadir/fish/vendor_functions.d $fish_function_path
            and echo function_path has $datadir
            contains -- $datadir/fish/vendor_completions.d $fish_complete_path
            and echo complete_path has $datadir
            contains -- $datadir/fish/vendor_conf.d $__fish_vendor_confdirs
            and echo vendor_conf has $datadir
        end
    ' $argv
end

expect-datadirs /usr/local/share /usr/share
# CHECK: function_path has /usr/local/share
# CHECK: complete_path has /usr/local/share
# CHECK: vendor_conf has /usr/local/share
# CHECK: function_path has /usr/share
# CHECK: complete_path has /usr/share
# CHECK: vendor_conf has /usr/share

XDG_DATA_DIRS= expect-datadirs /usr/local/share /usr/share
# CHECK: function_path has /usr/local/share
# CHECK: complete_path has /usr/local/share
# CHECK: vendor_conf has /usr/local/share
# CHECK: function_path has /usr/share
# CHECK: complete_path has /usr/share
# CHECK: vendor_conf has /usr/share

XDG_DATA_DIRS=/custom/path1:/custom/path2 expect-datadirs /custom/path1 /custom/path2
# CHECK: function_path has /custom/path1
# CHECK: complete_path has /custom/path1
# CHECK: vendor_conf has /custom/path1
# CHECK: function_path has /custom/path2
# CHECK: complete_path has /custom/path2
# CHECK: vendor_conf has /custom/path2
