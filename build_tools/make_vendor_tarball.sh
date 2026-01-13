#!/bin/sh

# Script to generate a tarball of vendored (downloaded) Rust dependencies
# and the cargo configuration to ensure they are used
# This tarball should be unpacked into a fish source directory
# Outputs to $FISH_ARTEFACT_PATH or ~/fish_built by default

# Exit on error
set -e

# Get the current directory, which we'll use for telling Cargo where to find the sources
wd="$PWD"

# Get the version from git-describe
VERSION=$(build_tools/git_version_gen.sh)

# The name of the prefix, which is the directory that you get when you untar
prefix="fish-$VERSION"

# The path where we will output the tar file
# Defaults to ~/fish_built
path=${FISH_ARTEFACT_PATH:-~/fish_built}/$prefix-vendor.tar

# Clean up stuff we've written before
rm -f "$path" "$path".xz

# Work in a temporary directory to avoid clobbering the source directory
PREFIX_TMPDIR=$(mktemp -d)
cd "$PREFIX_TMPDIR"

# Add .cargo/config.toml. This means that the caller may need to remove that file from the tarball.
# See e4674cd7b5f (.cargo/config.toml: exclude from tarball, 2025-01-12)

mkdir .cargo
{
    cat "$wd"/.cargo/config.toml
    cargo vendor --manifest-path "$wd/Cargo.toml"
} > .cargo/config.toml

tar cfvJ "$path".xz vendor .cargo

cd -
rm -r "$PREFIX_TMPDIR"

# Output what we did, and the sha256 hash
echo "Tarball written to $path".xz
openssl dgst -sha256 "$path".xz
