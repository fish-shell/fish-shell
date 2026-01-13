# localization: tier1
function wait
    set -l args (__fish_expand_pid_args $argv)
    and builtin wait $args
end
