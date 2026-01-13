#!/bin/sh
# Originally from the git sources (GIT-VERSION-GEN)

set -e

# Find the fish directory as two levels up from script directory.
FISH_BASE_DIR="$( cd "$( dirname "$( dirname "$0" )" )" && pwd )"

version=$(
    awk <"$FISH_BASE_DIR/Cargo.toml" -F'"' '$1 == "version = " { print $2 }'
)
if git_version=$(
    GIT_CEILING_DIRECTORIES=$FISH_BASE_DIR/.. \
        git -C "$FISH_BASE_DIR" describe --always --dirty 2>/dev/null); then
    if [ "$git_version" = "${git_version#"$version"}" ]; then
        version=$version-g$git_version
    else
        version=$git_version
    fi
fi

echo "$version"
