#
# Completions for the docker command
#

#
# Functions
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

function __fish_docker_all_containers --description "Show all containers"
  command docker ps -q -a
end


function __fish_docker_start_containers --description "Show the running containers"
  command docker ps -q
end

function __fish_docker_stop_containers --description "Show the exited containers"
  command docker ps -a | grep "Exit" | awk '{ print $1 }'
end

# All docker commands
complete -c docker -n '__fish_use_subcommand' -xa attach --description "Attach to a running container"
complete -c docker -n '__fish_use_subcommand' -xa build --description "Build a container from a Dockerfile"
complete -c docker -n '__fish_use_subcommand' -xa commit --description "Create a new image from a container's changes"
complete -c docker -n '__fish_use_subcommand' -xa cp --description "Copy files/folders from the containers filesystem to the host path"
complete -c docker -n '__fish_use_subcommand' -xa diff --description "Inspect changes on a container's filesystem"
complete -c docker -n '__fish_use_subcommand' -xa events --description "Get real time events from the server"
complete -c docker -n '__fish_use_subcommand' -xa export --description "Stream the contents of a container as a tar archive"
complete -c docker -n '__fish_use_subcommand' -xa history --description "Show the history of an image"
complete -c docker -n '__fish_use_subcommand' -xa images --description "List images"
complete -c docker -n '__fish_use_subcommand' -xa import --description "Create a new filesystem image from the contents of a tarball"
complete -c docker -n '__fish_use_subcommand' -xa info --description "Display system-wide information"
complete -c docker -n '__fish_use_subcommand' -xa insert --description "Insert a file in an image"
complete -c docker -n '__fish_use_subcommand' -xa inspect --description "Return low-level information on a container"
complete -c docker -n '__fish_use_subcommand' -xa kill --description "Kill a running container"
complete -c docker -n '__fish_use_subcommand' -xa load --description "Load an image from a tar archive"
complete -c docker -n '__fish_use_subcommand' -xa login --description "Register or Login to the docker registry server"
complete -c docker -n '__fish_use_subcommand' -xa logs --description "Fetch the logs of a container"
complete -c docker -n '__fish_use_subcommand' -xa port --description "Lookup the public-facing port which is NAT-ed to PRIVATE_PORT"
complete -c docker -n '__fish_use_subcommand' -xa ps --description "List containers"
complete -c docker -n '__fish_use_subcommand' -xa pull --description "Pull an image or a repository from the docker registry server"
complete -c docker -n '__fish_use_subcommand' -xa push --description "Push an image or a repository to the docker registry server"
complete -c docker -n '__fish_use_subcommand' -xa restart --description "Restart a running container"
complete -c docker -n '__fish_use_subcommand' -xa rm --description "Remove one or more containers"
complete -c docker -n '__fish_use_subcommand' -xa rmi --description "Remove one or more images"
complete -c docker -n '__fish_use_subcommand' -xa run --description "Run a command in a new container"
complete -c docker -n '__fish_use_subcommand' -xa save --description "Save an image to a tar archive"
complete -c docker -n '__fish_use_subcommand' -xa search --description "Search for an image in the docker index"
complete -c docker -n '__fish_use_subcommand' -xa start --description "Start a stopped container"
complete -c docker -n '__fish_use_subcommand' -xa stop --description "Stop a running container"
complete -c docker -n '__fish_use_subcommand' -xa tag --description "Tag an image into a repository"
complete -c docker -n '__fish_use_subcommand' -xa top --description "Lookup the running processes of a container"
complete -c docker -n '__fish_use_subcommand' -xa version --description "Show the docker version information"
complete -c docker -n '__fish_use_subcommand' -xa wait --description "Block until a container stops, then print its exit code"

#
# docker ps
#
complete -c docker -f -n '__fish_docker_using_command ps' -a '(__fish_docker_all_containers)' --description "Docker container"
complete -c docker -s a -l all            --description "Show all containers. Only running containers are shown by default."
complete -c docker -s q -l quiet==false   --description "Only display numeric IDs"
complete -c docker -s s -l size=false     --description "Display sizes"
complete -c docker -s n                   --description "Show n last created containers, include non-running ones. [-1]"
complete -c docker      -l before-id=""   --description "Show only container created before Id, include non-running ones."
complete -c docker      -l no-trunc=false --description "Show all containers. Only running containers are shown by default."
complete -c docker      -l since-id=""    --description "Show only containers created since Id, include non-running ones."

#
# docker start
#
complete -c docker -f -n '__fish_docker_using_command start' -a '(__fish_docker_stop_containers)' --description "Stopped container"

#
# docker stop
#
complete -c docker -f -n '__fish_docker_using_command stop' -a '(__fish_docker_start_containers)' --description "Container running"

#
# docker restart
#
complete -c docker -f -n '__fish_docker_using_command restart' -a '(__fish_docker_all_containers)' --description "Docker container"
