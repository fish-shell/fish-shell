#!/bin/sh

# Script to generate a tarball
# Outputs to $FISH_ARTEFACT_PATH or ~/fish_built by default

# Example usage:
#
#   build_tools/make_tarball.sh :/!.cargo/config.toml

# Exit on error
set -e

# Get the version
VERSION=$(build_tools/git_version_gen.sh)

prefix=fish-$VERSION
path=${FISH_ARTEFACT_PATH:-~/fish_built}/$prefix.tar.xz

git archive \
    --prefix="$prefix/" \
    HEAD "$@" |
    xz >"$path"

# Output what we did, and the sha256 hash
echo "Tarball written to $path"
openssl dgst -sha256 "$path"
