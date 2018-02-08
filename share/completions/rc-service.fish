# First argument is the names of the service, i.e. a file in /etc/init.d
complete -c rc-service -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
    -xa "(__fish_print_service_names)" -d "Service name"

# The second argument is what action to take with the service
complete -c rc-service -n "test (__fish_number_of_cmd_args_wo_opts) -gt 1" \
    -xa "start stop restart"

# Complete rc-service the options
complete -c rc-service -s e -l exists -d 'Tests if the service exists or not'
complete -c rc-service -s i -l ifexists \
    -d 'If the service exists, then run the command'
complete -c rc-service -s l -l list -d 'List all available services'
complete -c rc-service -s r -l resolve \
    -d 'Resolve the service name to an init script'
complete -c rc-service -s h -l help -d 'Display the help output'
complete -c rc-service -s C -l nocolor -d 'Disable color output'
complete -c rc-service -s V -l version -d 'Display software version'
complete -c rc-service -s v -l verbose -d 'Run verbosely'
complete -c rc-service -s q -l quiet -d 'Run quietly (Does not affect errors)'
