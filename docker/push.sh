#!/bin/sh

usage() {
    cat << EOF
Usage: $(basename "$0") DOCKERFILE
EOF
    exit 1
}

set -e

dockerfile=$1
test -n "$dockerfile" || usage

workspace_root=$(cd "$( dirname "$0" )"/.. >/dev/null && pwd)
. "$workspace_root"/docker/lib.sh

docker_build "$dockerfile" --cache-to=type=inline
docker push "$(image_tagname "$dockerfile")"
