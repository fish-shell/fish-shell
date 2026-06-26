#!/bin/sh

usage() {
    cat << EOF
Usage: $(basename "$0") [--shell-before] [--shell-after] DOCKERFILE
Options:
    --shell-before    Before the tests start, run a bash shell
    --shell-after     After the tests end, run a bash shell
    --lint, --no-lint Enable/disable linting and failure on warnings
EOF
    exit 1
}

set -e

DOCKER_EXTRA_ARGS=""

# Parse args.
while [ $# -gt 1 ]; do
    case "$1" in
        --shell-before)
            DOCKER_EXTRA_ARGS="$DOCKER_EXTRA_ARGS --env FISH_RUN_SHELL_BEFORE_TESTS=1"
            ;;
        --shell-after)
            DOCKER_EXTRA_ARGS="$DOCKER_EXTRA_ARGS --env FISH_RUN_SHELL_AFTER_TESTS=1"
            ;;
        --lint)
            DOCKER_EXTRA_ARGS="$DOCKER_EXTRA_ARGS --env FISH_CHECK_LINT=true"
            ;;
        --no-lint)
            DOCKER_EXTRA_ARGS="$DOCKER_EXTRA_ARGS --env FISH_CHECK_LINT=false"
            ;;
        *)
            usage
            ;;
    esac
    shift
done

dockerfile=$1
test -n "$dockerfile" || usage

workspace_root=$(cd "$( dirname "$0" )"/.. >/dev/null && pwd)

. "$workspace_root"/docker/lib.sh

docker_build "$dockerfile"

# Run tests in it, allowing them to fail without failing this script.
# shellcheck disable=SC2046  # for the isatty snippet
# shellcheck disable=SC2086  # $DOCKER_EXTRA_ARGS should have globbing and splitting applied.
docker run -i $(isatty 0 && printf %s -t) \
    --mount type=bind,source="$workspace_root",target=/fish-source,readonly \
    $DOCKER_EXTRA_ARGS \
    "$(image_tagname "$dockerfile")"
