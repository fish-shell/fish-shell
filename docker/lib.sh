#!/bin/sh

# Get fish source directory.
set -e
# shellcheck disable=SC2154
test -n "$workspace_root"

image_tagname() {
    printf %s "ghcr.io/fish-shell/fish-ci/$(basename -s .Dockerfile "$1"):latest"
}

docker_build() {
    dockerfile=$1
    shift
    tagname=$(image_tagname "$dockerfile")
    docker pull "$tagname" || true
    DOCKER_BUILDKIT=1 \
        docker build \
            --cache-from "$tagname" \
            -t "$tagname" \
            -f "$dockerfile" \
            "$workspace_root"/docker/context/ \
            "$@"
}
