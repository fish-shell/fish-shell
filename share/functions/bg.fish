# localization: tier1
function bg
    set -l args (__fish_expand_pid_args $argv)
    and builtin bg $args
end
