# localization: tier1
function disown
    set -l args (__fish_expand_pid_args $argv)
    and builtin disown $args
end
