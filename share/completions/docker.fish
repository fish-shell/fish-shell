# In WSL, when the docker app is not yet started, "docker" is a script printing
# some error message on stdout instead of a sourceable script
set -l docker_completion "$(docker completion fish 2>/dev/null)"
and eval "$docker_completion"
