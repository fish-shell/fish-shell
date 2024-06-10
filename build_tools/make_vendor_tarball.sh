#!/bin/sh

# Script to generate a tarball of vendored (downloaded) Rust dependencies
# and the cargo configuration to ensure they are used
# This tarball should be unpacked into a fish source directory
# Outputs to $FISH_ARTEFACT_PATH or ~/fish_built by default

# Exit on error
set -e

# We need GNU tar as that supports the --mtime and --transform options
TAR=notfound
for try in tar gtar gnutar; do
  if $try -Pcf /dev/null --mtime now /dev/null >/dev/null 2>&1; then
    TAR=$try
    break
  fi
done

if [ "$TAR" = "notfound" ]; then
  echo 'No suitable tar (supporting --mtime) found as tar/gtar/gnutar in PATH'
  exit 1
fi

# Get the current directory, which we'll use for telling Cargo where to find the sources
wd="$PWD"

# Get the version from git-describe
VERSION=$(git describe --dirty 2>/dev/null)

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

mkdir .cargo
cargo vendor --manifest-path "$wd/Cargo.toml" > .cargo/config

tar cfvJ $path.xz vendor .cargo

cd -
rm -r "$PREFIX_TMPDIR"

# Output what we did, and the sha256 hash
echo "Tarball written to $path".xz
openssl dgst -sha256 "$path".xz
