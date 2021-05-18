function __fish_complete_rc-update_actions
    set -l actions add \
        'Add the service to the runlevel or the current one if non given'
    set -l actions $actions del \
        'Delete the service from the runlevel or the current one if non given'
    set -l actions $actions show \
        'Show all enabled services and the runlevels they belong to'
    printf "%s\t%s\n" $actions
end

function __fish_complete_rc-update_runlevels
    set -l levels sysinit \
        'First startup runlevel' \
        boot \
        'Second startup runlevel' \
        default \
        'Last startup runlevel' \
        shutdown \
        'Runlevel for shutting down'
    printf "%s\t%s\n" $levels
end

# The first argument is what action to take with the service
complete -c rc-update -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
    -xa "(__fish_complete_rc-update_actions)"

# The second argument is the names of the service, i.e. a file in /etc/init.d
complete -c rc-update -n "test (__fish_number_of_cmd_args_wo_opts) = 2" \
    -xa "(__fish_print_service_names)" -d "Service name"

# The third argument is the names of the service, i.e. a file in /etc/init.d
complete -c rc-update -n "test (__fish_number_of_cmd_args_wo_opts) = 3" \
    -xa "(__fish_complete_rc-update_runlevels)"
