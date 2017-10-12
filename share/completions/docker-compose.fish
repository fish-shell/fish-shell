#
# Completions for docker-compose
#

function __fish_docker_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end
  return 1
end

function __fish_docker_compose_file_path -d \
        'Get the next docker-compose.yml file in the folder parent path.'
    set -l path $PWD
    while not test -e $path/docker-compose.yml
        if test $path = '/'
            return
        end

        set path (realpath $path/..)
    end

    echo $path/docker-compose.yml
end

function __fish_docker_compose_file_version -d \
        'Get the version of a docker-compose.yml file.'
    cat (__fish_docker_compose_file_path) \
        | string match -r '^version:(\\s*)["\']?[0-9]*["\']?' \
        | string match -ar '[0-9]'

    if test $status -ne 0
        echo '1'
    end
end

function __fish_docker_compose_all_services -d \
        'List all services in docker-compose.yml.'
    set -l path (__fish_docker_compose_file_path)
    set -l file_version (__fish_docker_compose_file_version)
    set -l chars 'a-zA-Z0-9_.-'

    switch $file_version
        case '1'
            cat $path | string match -er '^[a-zA-Z]' | string replace -a ":" ""
        case '*'
            # TODO: currently this only finds services that are indented with
            # 2 spaces. Make it work with any indentation.
            cat $path | command sed -n '/^services:/,/^\w/p' \
                | string match -ar "^\s{2,4}[$chars]*:" \
                | string replace -ar "[^$chars]" ""
    end
end


# Delete old completions
complete -c docker-compose -e

# All docker-compose commands
complete -c docker-compose -n '__fish_use_subcommand' -xa build   -d "Build or rebuild services"
complete -c docker-compose -n '__fish_use_subcommand' -xa bundle  -d "Generate a Docker bundle from the Compose file"
complete -c docker-compose -n '__fish_use_subcommand' -xa config  -d "Validate and print compose configuration"
complete -c docker-compose -n '__fish_use_subcommand' -xa create  -d "Create containers without starting them"
complete -c docker-compose -n '__fish_use_subcommand' -xa down    -d "Stop and remove all container resources"
complete -c docker-compose -n '__fish_use_subcommand' -xa events  -d "Monitor events from containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa exec    -d "Execute a command in a running container"
complete -c docker-compose -n '__fish_use_subcommand' -xa help    -d "Get help on a command"
complete -c docker-compose -n '__fish_use_subcommand' -xa images  -d "List images"
complete -c docker-compose -n '__fish_use_subcommand' -xa kill    -d "Kill containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa logs    -d "View output from containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa pause   -d "Pause services"
complete -c docker-compose -n '__fish_use_subcommand' -xa port    -d "Print the public port for a port binding"
complete -c docker-compose -n '__fish_use_subcommand' -xa ps      -d "List containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa pull    -d "Pulls service images"
complete -c docker-compose -n '__fish_use_subcommand' -xa push    -d "Push service images"
complete -c docker-compose -n '__fish_use_subcommand' -xa restart -d "Restart services"
complete -c docker-compose -n '__fish_use_subcommand' -xa rm      -d "Remove stopped containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa run     -d "Run a one-off command"
complete -c docker-compose -n '__fish_use_subcommand' -xa scale   -d "Set number of containers for a service"
complete -c docker-compose -n '__fish_use_subcommand' -xa start   -d "Start services"
complete -c docker-compose -n '__fish_use_subcommand' -xa stop    -d "Stop services"
complete -c docker-compose -n '__fish_use_subcommand' -xa top     -d "Display the running processes"
complete -c docker-compose -n '__fish_use_subcommand' -xa unpause -d "Unpause services"
complete -c docker-compose -n '__fish_use_subcommand' -xa up      -d "Create and start containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa version -d "Show the Docker-Compose version information"

# docker-compose commands that take services
for subcmd in build create events exec images kill logs pause port ps pull push restart rm run scale start stop top unpause up
    complete -c docker-compose -f -n "__fish_docker_using_command $subcmd" \
        -a '(__fish_docker_compose_all_services)' \
        -d "Docker compose service"
end


#
#   FLAGS
#

# build
complete -c docker-compose -n "__fish_docker_using_command build" -l build-arg -d "Set build-time variables for one service"
complete -c docker-compose -n "__fish_docker_using_command build" -l force-rm  -d "Always remove intermediate containers"
complete -c docker-compose -n "__fish_docker_using_command build" -l no-cache  -d "Do not use cache when building the image"
complete -c docker-compose -n "__fish_docker_using_command build" -l pull      -d "Always attempt to pull a newer version of the image"

# bundle
complete -c docker-compose -n "__fish_docker_using_command bundle"      -l push-images -d "Automatically push images for any services which have a `build` option specified"
complete -c docker-compose -n "__fish_docker_using_command bundle" -s o -l output      -d "Path to write the bundle file to. Defaults to '<project name>.dab'"

# config
complete -c docker-compose -n "__fish_docker_using_command config" -s q -l quiet                 -d "Only validate the configuration, don't print anything"
complete -c docker-compose -n "__fish_docker_using_command config"      -l resolve-image-digests -d "Pin image tags to digests"
complete -c docker-compose -n "__fish_docker_using_command config"      -l services              -d "Print the service names, one per line"
complete -c docker-compose -n "__fish_docker_using_command config"      -l volumes               -d "Print the volume names, one per line"

# create
complete -c docker-compose -n "__fish_docker_using_command create" -l build          -d "Build images before creating containers"
complete -c docker-compose -n "__fish_docker_using_command create" -l force-recreate -d "Recreate containers even if their configruation and image haven't changed. Incompatible with --no-recreate"
complete -c docker-compose -n "__fish_docker_using_command create" -l no-build       -d "Don't build an image, even if it's missing"
complete -c docker-compose -n "__fish_docker_using_command create" -l no-recreate    -d "If containers already exist, don't recreate them. Incompatible with --force-recreate"

# down
complete -c docker-compose -n "__fish_docker_using_command down"      -l remove-orphans     -d "Remove containers for services not defined in the Compose file"
complete -c docker-compose -n "__fish_docker_using_command down"      -l rmi -a "all local" -d "Remove images, type may be one of: 'all' to remove all images, or 'local' to remove only images that don't have a custom name set by the 'image' field"
complete -c docker-compose -n "__fish_docker_using_command down" -s v -l volumes            -d "Remove data volumes"

# events
complete -c docker-compose -n "__fish_docker_using_command events" -l json -d "Output events as a stream of json objects"

# exec
complete -c docker-compose -n "__fish_docker_using_command exec"      -l index -a "1" -d "Index of the container if there are multiple instances of a service. Defaults to 1"
complete -c docker-compose -n "__fish_docker_using_command exec"      -l privileged   -d "Give extended privileges to the process"
complete -c docker-compose -n "__fish_docker_using_command exec" -s T                 -d "Disable pseudo-tty allocation. By default `docker-compose exec` allocates a TTY"
complete -c docker-compose -n "__fish_docker_using_command exec" -s d                 -d "Detached mode: Run command in the background"
complete -c docker-compose -n "__fish_docker_using_command exec" -s u -l user         -d "Run the command as this user"

# images
complete -c docker-compose -n "__fish_docker_using_command images" -s q -d "Only display IDs"

# kill
complete -c docker-compose -n "__fish_docker_using_command kill" -s s -d "SIGNAL to send to the container. Default signal is SIGKILL"

# logs
complete -c docker-compose -n "__fish_docker_using_command logs"      -l no-color  -d "Produce monochrome output"
complete -c docker-compose -n "__fish_docker_using_command logs"      -l tail      -d "Number of lines to show from the end of the logs for each container"
complete -c docker-compose -n "__fish_docker_using_command logs" -s f -l follow    -d "Follow log output"
complete -c docker-compose -n "__fish_docker_using_command logs" -s t -l timestaps -d "Show timestamps"

# port
complete -c docker-compose -n "__fish_docker_using_command port" -l index    -a "1"       -d "Index of the container if there are multiple instances of a service. Defaults to 1"
complete -c docker-compose -n "__fish_docker_using_command port" -l protocol -a "tcp udp" -d "Protocol to use, TCP or UDP. Defaults to TCP"

# ps
complete -c docker-compose -n "__fish_docker_using_command ps" -s q -d "Only display IDs"

# pull
complete -c docker-compose -n "__fish_docker_using_command pull" -l ignore-pull-failures -d "Pull what it can and ignores images with pull failures"
complete -c docker-compose -n "__fish_docker_using_command pull" -l parallel             -d "Pull multiple images in parallel"
complete -c docker-compose -n "__fish_docker_using_command pull" -l quiet                -d "Pull without printing progress information"

# push
complete -c docker-compose -n "__fish_docker_using_command push" -l ignore-push-failures -d "Push what it can and ignores images with push failures"

# restart
complete -c docker-compose -n "__fish_docker_using_command restart" -s t -l timeout -a "10" -d "Specify a shutdown timeout in seconds. Default 10"

# rm
complete -c docker-compose -n "__fish_docker_using_command rm" -s a -l all   -d "Also remove one-off containers created by docker-compose run"
complete -c docker-compose -n "__fish_docker_using_command rm" -s f -l force -d "Don't ask to confirm removal"
complete -c docker-compose -n "__fish_docker_using_command rm" -s s -l stop  -d "Stop the containers, if required, before removing"
complete -c docker-compose -n "__fish_docker_using_command rm" -s v          -d "Remove volumes associated with containers"

# run
complete -c docker-compose -n "__fish_docker_using_command run"      -l entrypoint    -d "Override the entrypoint of the image"
complete -c docker-compose -n "__fish_docker_using_command run"      -l name          -d "Assign a name to the container"
complete -c docker-compose -n "__fish_docker_using_command run"      -l no-deps       -d "Don't start linked services"
complete -c docker-compose -n "__fish_docker_using_command run"      -l rm            -d "Remove container after run. Ignored in detached mode"
complete -c docker-compose -n "__fish_docker_using_command run"      -l service-ports -d "Run command with the service's ports enabled and mapped to the host"
complete -c docker-compose -n "__fish_docker_using_command run" -s T                  -d "Disable pseudo-tty allocation. By default 'docker-compose run' allocates a TTY"
complete -c docker-compose -n "__fish_docker_using_command run" -s d                  -d "Detached mode: Run container in the background, print new container name"
complete -c docker-compose -n "__fish_docker_using_command run" -s e                  -d "Set an environment variable (can be used multiple times)"
complete -c docker-compose -n "__fish_docker_using_command run" -s p -l publish       -d "Publish a container's port(s) to the host"
complete -c docker-compose -n "__fish_docker_using_command run" -s u -l user          -d "Run as a specified username or uid"
complete -c docker-compose -n "__fish_docker_using_command run" -s w -l workdir       -d "Working directory inside the container"
complete -c docker-compose -n "__fish_docker_using_command run" -s v -l volume        -d "Bind mount a volume (default [])"

# scale
complete -c docker-compose -n "__fish_docker_using_command scale" -s t -l timeout -a "10" -d "Specify a shutdown timeout in seconds. Default 10"

# stop
complete -c docker-compose -n "__fish_docker_using_command stop" -s t -l timeout -a "10" -d "Specify a shutdown timeout in seconds. Default 10"

# up
complete -c docker-compose -n "__fish_docker_using_command up" -s d                            -d "Detached mode: Run containers in the background, print new container names"
complete -c docker-compose -n "__fish_docker_using_command up"      -l no-color                -d "Produce monochrome output"
complete -c docker-compose -n "__fish_docker_using_command up"      -l no-deps                 -d "Don't start linked services"
complete -c docker-compose -n "__fish_docker_using_command up"      -l force-recreate          -d "Recreate containers even if their configuration and image haven't changed"
complete -c docker-compose -n "__fish_docker_using_command up"      -l no-recreate             -d "If containers already exist, don't recreate them. Incompatible with --force-recreate"
complete -c docker-compose -n "__fish_docker_using_command up"      -l no-build                -d "Don't build an image, even if it's missing"
complete -c docker-compose -n "__fish_docker_using_command up"      -l build                   -d "Build images before starting containers"
complete -c docker-compose -n "__fish_docker_using_command up"      -l abort-on-container-exit -d "Stop all containers if any container was stopped. Incompatible with -d"
complete -c docker-compose -n "__fish_docker_using_command up" -s t -l timeout                 -d "Use this timeout in seconds for container shutdown when attached or when containers are already running. Default 10"
complete -c docker-compose -n "__fish_docker_using_command up"      -l remove-orphans          -d "Remove containers for services not defined in the Compose file"
complete -c docker-compose -n "__fish_docker_using_command up"      -l exit-code-from          -d "Return the exit code of the selected service container. Implies --abort-on-container-exit"
complete -c docker-compose -n "__fish_docker_using_command up"      -l scale                   -d "Scale SERVICE to NUM instances. Overrides the `scale` setting in the Compose file if present"

# version
complete -c docker-compose -n "__fish_docker_using_command version" -l short -d "Shows only Compose's version number"
