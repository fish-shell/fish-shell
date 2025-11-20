#!/bin/sh

# Script to generate a tarball
# Outputs to $FISH_ARTEFACT_PATH or ~/fish_built by default

# Exit on error
set -e

# Get the version
VERSION=$(build_tools/git_version_gen.sh --stdout 2>/dev/null)

prefix=fish-$VERSION
path=${FISH_ARTEFACT_PATH:-~/fish_built}/$prefix.tar.xz

git archive \
    --prefix="$prefix/" \
    --add-virtual-file="$prefix/version:$VERSION" \
    HEAD |
    xz >"$path"

# Output what we did, and the sha256 hash
echo "Tarball written to $path"
openssl dgst -sha256 "$path"
