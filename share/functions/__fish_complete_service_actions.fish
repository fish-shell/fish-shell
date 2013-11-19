function __fish_complete_service_actions -d "Print a list of all basic service \
        actions"
    set -l actions start 'Start the service'
    set -l actions $actions stop 'Stop the service'
    set -l actions $actions restart 'Restart the service'
    printf "%s\t%s\n" $actions
end
