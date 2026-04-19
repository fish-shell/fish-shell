#RUN: fish=%fish %fish %s

# Cygwin/MSYS path handling differs from the XDG fallback fish uses elsewhere.
# REQUIRES: %fish -c "not __fish_is_cygwin"

set -l dir (mktemp -d)
mkdir -p $dir/xdg-config $dir/xdg-data $dir/xdg-cache $dir/xdg-runtime

set -e FISH_UNIT_TESTS_RUNNING
env HOME=$dir XDG_CONFIG_HOME=$dir/xdg-config XDG_DATA_HOME=$dir/xdg-data XDG_CACHE_HOME=$dir/xdg-cache XDG_RUNTIME_DIR=$dir/xdg-runtime $fish -c '
    contains -- /usr/share/fish/vendor_functions.d $fish_function_path
    and echo unset_function_path_has_usr_share
    contains -- /usr/share/fish/vendor_completions.d $fish_complete_path
    and echo unset_complete_path_has_usr_share
    contains -- /usr/share/fish/vendor_conf.d $__fish_vendor_confdirs
    and echo unset_vendor_conf_has_usr_share
'
# CHECK: unset_function_path_has_usr_share
# CHECK: unset_complete_path_has_usr_share
# CHECK: unset_vendor_conf_has_usr_share

env HOME=$dir XDG_CONFIG_HOME=$dir/xdg-config XDG_DATA_HOME=$dir/xdg-data XDG_CACHE_HOME=$dir/xdg-cache XDG_RUNTIME_DIR=$dir/xdg-runtime XDG_DATA_DIRS= $fish -c '
    contains -- /usr/share/fish/vendor_functions.d $fish_function_path
    and echo empty_function_path_has_usr_share
    contains -- /usr/share/fish/vendor_completions.d $fish_complete_path
    and echo empty_complete_path_has_usr_share
    contains -- /usr/share/fish/vendor_conf.d $__fish_vendor_confdirs
    and echo empty_vendor_conf_has_usr_share
'
# CHECK: empty_function_path_has_usr_share
# CHECK: empty_complete_path_has_usr_share
# CHECK: empty_vendor_conf_has_usr_share

rm -rf $dir
