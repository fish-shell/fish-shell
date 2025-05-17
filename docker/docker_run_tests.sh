#!/bin/sh

usage() {
    cat << EOF
Usage: $(basename "$0") [--shell-before] [--shell-after] DOCKERFILE
Options:
    --shell-before   Before the tests start, run a bash shell
    --shell-after    After the tests end, run a bash shell
EOF
    exit 1
}

DOCKER_EXTRA_ARGS=""

export DOCKER_BUILDKIT=1

# Exit on failure.
set -e

# Get fish source directory.
FISH_SRC_DIR=$(cd "$( dirname "$0" )"/.. >/dev/null && pwd)

# Parse args.
while [ $# -gt 1 ]; do
    case "$1" in
        --shell-before)
            DOCKER_EXTRA_ARGS="$DOCKER_EXTRA_ARGS --env FISH_RUN_SHELL_BEFORE_TESTS=1"
            ;;
        --shell-after)
            DOCKER_EXTRA_ARGS="$DOCKER_EXTRA_ARGS --env FISH_RUN_SHELL_AFTER_TESTS=1"
            ;;
        *)
            usage
            ;;
    esac
    shift
done

DOCKERFILE="$1"
test -n "$DOCKERFILE" || usage

# Construct a docker image.
IMG_TAGNAME="ghcr.io/fish-shell/fish-ci/$(basename -s .Dockerfile "$DOCKERFILE"):latest"
docker build \
    -t "$IMG_TAGNAME" \
    -f "$DOCKERFILE" \
    "$FISH_SRC_DIR"/docker/context/

# Run tests in it, allowing them to fail without failing this script.
# shellcheck disable=SC2086  # $DOCKER_EXTRA_ARGS should have globbing and splitting applied.
docker run -it \
    --mount type=bind,source="$FISH_SRC_DIR",target=/fish-source,readonly \
    $DOCKER_EXTRA_ARGS \
    "$IMG_TAGNAME"
