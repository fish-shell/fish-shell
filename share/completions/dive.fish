# Completion for dive: https://github.com/wagoodman/dive

# Builtin options and subcommands
dive completion fish | source

# Arguments
function __fish_docker_or_podman_image_tags
    command -v docker >/dev/null
    set --local docker_status $status
    command -v podman >/dev/null
    set --local podman_status $status

    if test $docker_status -eq 0 && test $podman_status -eq 0
        docker images --format '{{.Repository}}:{{.Tag}}' 2>/dev/null | command grep -v '<none>' | sed 's#^#docker://#'
        podman images --format '{{.Repository}}:{{.Tag}}' 2>/dev/null | command grep -v '<none>' | sed 's#^#podman://#'
    else if test $docker_status -eq 0
        docker images --format '{{.Repository}}:{{.Tag}}' 2>/dev/null | command grep -v '<none>'
    else if test $podman_status -eq 0
        podman images --format '{{.Repository}}:{{.Tag}}' 2>/dev/null | command grep -v '<none>'
    end
end
complete -c dive -xa "(__fish_docker_or_podman_image_tags)"
