# localization: tier1
function fg
    set -l args (__fish_expand_pid_args $argv)
    and builtin fg $args[-1]
end
