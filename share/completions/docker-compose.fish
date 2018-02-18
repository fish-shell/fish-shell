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

function __fish_docker_compose_file_path --description \
        'Get the next docker-compose.yml file in the folder parent path.'
    set -l path (pwd)
    while not test -e $path/docker-compose.yml
        if test $path = '/'
            return
        end

        set path (realpath $path/..)
    end

    echo $path/docker-compose.yml
end

function __fish_docker_compose_file_version --description \
        'Get the version of a docker-compose.yml file.'
    cat (__fish_docker_compose_file_path) \
        | command grep '^version:\(\s*\)["\']\?[.0-9]*["\']\?' \
        | command grep -o '[.0-9]*'

    if test $status -ne 0
        echo '1'
    end
end

function __fish_docker_compose_all_services --description \
        'List all services in docker-compose.yml.'
    set -l path (__fish_docker_compose_file_path)
    set -l file_version (__fish_docker_compose_file_version)
    set -l chars 'a-zA-Z0-9_.-'

    switch $file_version
        case '1'
            cat $path | command grep '^[a-zA-Z]' | command sed 's/://'
        case '*'
            # TODO: currently this only finds services that are indented with
            # 2 spaces. Make it work with any indentation.
            cat $path | command sed -n '/^services:/,/^\w/p' \
                | command grep "^  [$chars]*:" \
                | command sed "s/[^$chars]//g"
    end
end


# Delete old completions
complete -c docker-compose -e

# Base docker-compose flags
complete -c docker-compose -s f -l file -r         -d "Specify an alternate compose file"
complete -c docker-compose -s p -l project-name -x -d "Specify an alternate project name"
complete -c docker-compose -l verbose              -d "Show more output"
complete -c docker-compose -s H -l host -x         -d "Daemon socket to connect to"
complete -c docker-compose -l tls                  -d "Use TLS; implied by --tlsverify"
complete -c docker-compose -l tlscacert -r         -d "Trust certs signed only by this CA"
complete -c docker-compose -l tlscert -r           -d "Path to TLS certificate file"
complete -c docker-compose -l tlskey -r            -d "Path to TLS key file"
complete -c docker-compose -l tlsverify            -d "Use TLS and verify the remote"
complete -c docker-compose -l skip-hostname-check  -d "Don't check the daemon's hostname against the name specified in the client certificate (for example if your docker host is an IP address)"
complete -c docker-compose -s h -l help            -d "Print usage"
complete -c docker-compose -s v -l version         -d "Print version and exit"

# All docker-compose commands
complete -c docker-compose -n '__fish_use_subcommand' -xa build   --description "Build or rebuild services"
complete -c docker-compose -n '__fish_use_subcommand' -xa bundle  --description "Generate a Docker bundle from the Compose file"
complete -c docker-compose -n '__fish_use_subcommand' -xa config  --description "Validate and print compose configuration"
complete -c docker-compose -n '__fish_use_subcommand' -xa create  --description "Create containers without starting them"
complete -c docker-compose -n '__fish_use_subcommand' -xa down    --description "Stop and remove all container resources"
complete -c docker-compose -n '__fish_use_subcommand' -xa events  --description "Monitor events from containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa exec    --description "Execute a command in a running container"
complete -c docker-compose -n '__fish_use_subcommand' -xa help    --description "Get help on a command"
complete -c docker-compose -n '__fish_use_subcommand' -xa images  --description "List images"
complete -c docker-compose -n '__fish_use_subcommand' -xa kill    --description "Kill containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa logs    --description "View output from containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa pause   --description "Pause services"
complete -c docker-compose -n '__fish_use_subcommand' -xa port    --description "Print the public port for a port binding"
complete -c docker-compose -n '__fish_use_subcommand' -xa ps      --description "List containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa pull    --description "Pulls service images"
complete -c docker-compose -n '__fish_use_subcommand' -xa push    --description "Push service images"
complete -c docker-compose -n '__fish_use_subcommand' -xa restart --description "Restart services"
complete -c docker-compose -n '__fish_use_subcommand' -xa rm      --description "Remove stopped containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa run     --description "Run a one-off command"
complete -c docker-compose -n '__fish_use_subcommand' -xa scale   --description "Set number of containers for a service"
complete -c docker-compose -n '__fish_use_subcommand' -xa start   --description "Start services"
complete -c docker-compose -n '__fish_use_subcommand' -xa stop    --description "Stop services"
complete -c docker-compose -n '__fish_use_subcommand' -xa top     --description "Display the running processes"
complete -c docker-compose -n '__fish_use_subcommand' -xa unpause --description "Unpause services"
complete -c docker-compose -n '__fish_use_subcommand' -xa up      --description "Create and start containers"
complete -c docker-compose -n '__fish_use_subcommand' -xa version --description "Show the Docker-Compose version information"

# docker-compose commands that take services
for subcmd in build create events exec images kill logs pause port ps pull push restart rm run scale start stop top unpause up
    complete -c docker-compose -f -n "__fish_docker_using_command $subcmd" \
        -a '(__fish_docker_compose_all_services)' \
        --description "Docker compose service"
end


#
#   FLAGS
#

# build
complete -c docker-compose -n "__fish_docker_using_command build" -l build-arg --description "Set build-time variables for one service"
complete -c docker-compose -n "__fish_docker_using_command build" -l force-rm  --description "Always remove intermediate containers"
complete -c docker-compose -n "__fish_docker_using_command build" -l no-cache  --description "Do not use cache when building the image"
complete -c docker-compose -n "__fish_docker_using_command build" -l pull      --description "Always attempt to pull a newer version of the image"

# bundle
complete -c docker-compose -n "__fish_docker_using_command bundle"      -l push-images --description "Automatically push images for any services which have a `build` option specified"
complete -c docker-compose -n "__fish_docker_using_command bundle" -s o -l output      --description "Path to write the bundle file to. Defaults to '<project name>.dab'"

# config
complete -c docker-compose -n "__fish_docker_using_command config" -s q -l quiet                 --description "Only validate the configuration, don't print anything"
complete -c docker-compose -n "__fish_docker_using_command config"      -l resolve-image-digests --description "Pin image tags to digests"
complete -c docker-compose -n "__fish_docker_using_command config"      -l services              --description "Print the service names, one per line"
complete -c docker-compose -n "__fish_docker_using_command config"      -l volumes               --description "Print the volume names, one per line"

# create
complete -c docker-compose -n "__fish_docker_using_command create" -l build          --description "Build images before creating containers"
complete -c docker-compose -n "__fish_docker_using_command create" -l force-recreate --description "Recreate containers even if their configruation and image haven't changed. Incompatible with --no-recreate"
complete -c docker-compose -n "__fish_docker_using_command create" -l no-build       --description "Don't build an image, even if it's missing"
complete -c docker-compose -n "__fish_docker_using_command create" -l no-recreate    --description "If containers already exist, don't recreate them. Incompatible with --force-recreate"

# down
complete -c docker-compose -n "__fish_docker_using_command down"      -l remove-orphans     --description "Remove containers for services not defined in the Compose file"
complete -c docker-compose -n "__fish_docker_using_command down"      -l rmi -a "all local" --description "Remove images, type may be one of: 'all' to remove all images, or 'local' to remove only images that don't have a custom name set by the 'image' field"
complete -c docker-compose -n "__fish_docker_using_command down" -s v -l volumes            --description "Remove data volumes"

# events
complete -c docker-compose -n "__fish_docker_using_command events" -l json --description "Output events as a stream of json objects"

# exec
complete -c docker-compose -n "__fish_docker_using_command exec"      -l index -a "1" --description "Index of the container if there are multiple instances of a service. Defaults to 1"
complete -c docker-compose -n "__fish_docker_using_command exec"      -l privileged   --description "Give extended privileges to the process"
complete -c docker-compose -n "__fish_docker_using_command exec" -s T                 --description "Disable pseudo-tty allocation. By default `docker-compose exec` allocates a TTY"
complete -c docker-compose -n "__fish_docker_using_command exec" -s d                 --description "Detached mode: Run command in the background"
complete -c docker-compose -n "__fish_docker_using_command exec" -s u -l user         --description "Run the command as this user"

# images
complete -c docker-compose -n "__fish_docker_using_command images" -s q --description "Only display IDs"

# kill
complete -c docker-compose -n "__fish_docker_using_command kill" -s s --description "SIGNAL to send to the container. Default signal is SIGKILL"

# logs
complete -c docker-compose -n "__fish_docker_using_command logs"      -l no-color  --description "Produce monochrome output"
complete -c docker-compose -n "__fish_docker_using_command logs"      -l tail      --description "Number of lines to show from the end of the logs for each container"
complete -c docker-compose -n "__fish_docker_using_command logs" -s f -l follow    --description "Follow log output"
complete -c docker-compose -n "__fish_docker_using_command logs" -s t -l timestaps --description "Show timestamps"

# port
complete -c docker-compose -n "__fish_docker_using_command port" -l index    -a "1"       --description "Index of the container if there are multiple instances of a service. Defaults to 1"
complete -c docker-compose -n "__fish_docker_using_command port" -l protocol -a "tcp udp" --description "Protocol to use, TCP or UDP. Defaults to TCP"

# ps
complete -c docker-compose -n "__fish_docker_using_command ps" -s q --description "Only display IDs"

# pull
complete -c docker-compose -n "__fish_docker_using_command pull" -l ignore-pull-failures --description "Pull what it can and ignores images with pull failures"
complete -c docker-compose -n "__fish_docker_using_command pull" -l parallel             --description "Pull multiple images in parallel"
complete -c docker-compose -n "__fish_docker_using_command pull" -l quiet                --description "Pull without printing progress information"

# push
complete -c docker-compose -n "__fish_docker_using_command push" -l ignore-push-failures --description "Push what it can and ignores images with push failures"

# restart
complete -c docker-compose -n "__fish_docker_using_command restart" -s t -l timeout -a "10" --description "Specify a shutdown timeout in seconds. Default 10"

# rm
complete -c docker-compose -n "__fish_docker_using_command rm" -s a -l all   --description "Also remove one-off containers created by docker-compose run"
complete -c docker-compose -n "__fish_docker_using_command rm" -s f -l force --description "Don't ask to confirm removal"
complete -c docker-compose -n "__fish_docker_using_command rm" -s s -l stop  --description "Stop the containers, if required, before removing"
complete -c docker-compose -n "__fish_docker_using_command rm" -s v          --description "Remove volumes associated with containers"

# run
complete -c docker-compose -n "__fish_docker_using_command run"      -l entrypoint    --description "Override the entrypoint of the image"
complete -c docker-compose -n "__fish_docker_using_command run"      -l name          --description "Assign a name to the container"
complete -c docker-compose -n "__fish_docker_using_command run"      -l no-deps       --description "Don't start linked services"
complete -c docker-compose -n "__fish_docker_using_command run"      -l rm            --description "Remove container after run. Ignored in detached mode"
complete -c docker-compose -n "__fish_docker_using_command run"      -l service-ports --description "Run command with the service's ports enabled and mapped to the host"
complete -c docker-compose -n "__fish_docker_using_command run" -s T                  --description "Disable pseudo-tty allocation. By default 'docker-compose run' allocates a TTY"
complete -c docker-compose -n "__fish_docker_using_command run" -s d                  --description "Detached mode: Run container in the background, print new container name"
complete -c docker-compose -n "__fish_docker_using_command run" -s e                  --description "Set an environment variable (can be used multiple times)"
complete -c docker-compose -n "__fish_docker_using_command run" -s p -l publish       --description "Publish a container's port(s) to the host"
complete -c docker-compose -n "__fish_docker_using_command run" -s u -l user          --description "Run as a specified username or uid"
complete -c docker-compose -n "__fish_docker_using_command run" -s w -l workdir       --description "Working directory inside the container"
complete -c docker-compose -n "__fish_docker_using_command run" -s v -l volume        --description "Bind mount a volume (default [])"

# scale
complete -c docker-compose -n "__fish_docker_using_command scale" -s t -l timeout -a "10" --description "Specify a shutdown timeout in seconds. Default 10"

# stop
complete -c docker-compose -n "__fish_docker_using_command stop" -s t -l timeout -a "10" --description "Specify a shutdown timeout in seconds. Default 10"

# up
complete -c docker-compose -n "__fish_docker_using_command up" -s d                            --description "Detached mode: Run containers in the background, print new container names"
complete -c docker-compose -n "__fish_docker_using_command up"      -l no-color                --description "Produce monochrome output"
complete -c docker-compose -n "__fish_docker_using_command up"      -l no-deps                 --description "Don't start linked services"
complete -c docker-compose -n "__fish_docker_using_command up"      -l force-recreate          --description "Recreate containers even if their configuration and image haven't changed"
complete -c docker-compose -n "__fish_docker_using_command up"      -l no-recreate             --description "If containers already exist, don't recreate them. Incompatible with --force-recreate"
complete -c docker-compose -n "__fish_docker_using_command up"      -l no-build                --description "Don't build an image, even if it's missing"
complete -c docker-compose -n "__fish_docker_using_command up"      -l build                   --description "Build images before starting containers"
complete -c docker-compose -n "__fish_docker_using_command up"      -l abort-on-container-exit --description "Stop all containers if any container was stopped. Incompatible with -d"
complete -c docker-compose -n "__fish_docker_using_command up" -s t -l timeout                 --description "Use this timeout in seconds for container shutdown when attached or when containers are already running. Default 10"
complete -c docker-compose -n "__fish_docker_using_command up"      -l remove-orphans          --description "Remove containers for services not defined in the Compose file"
complete -c docker-compose -n "__fish_docker_using_command up"      -l exit-code-from          --description "Return the exit code of the selected service container. Implies --abort-on-container-exit"
complete -c docker-compose -n "__fish_docker_using_command up"      -l scale                   --description "Scale SERVICE to NUM instances. Overrides the `scale` setting in the Compose file if present"

# version
complete -c docker-compose -n "__fish_docker_using_command version" -l short --description "Shows only Compose's version number"
