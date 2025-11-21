#!/bin/sh

# Script to generate a tarball
# Outputs to $FISH_ARTEFACT_PATH or ~/fish_built by default

# Exit on error
set -e

# We will generate a tarball with a prefix "fish-VERSION"

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

# Get the current directory, which we'll use for symlinks
wd="$PWD"

# Get the version
VERSION=$(build_tools/git_version_gen.sh --stdout 2>/dev/null)

# The name of the prefix, which is the directory that you get when you untar
prefix="fish-$VERSION"

# The path where we will output the tar file
# Defaults to ~/fish_built
path=${FISH_ARTEFACT_PATH:-~/fish_built}/$prefix.tar

# Clean up stuff we've written before
rm -f "$path" "$path".xz

# git starts the archive
git archive --format=tar --prefix="$prefix"/ HEAD > "$path"

# tarball out the documentation, generate a version file
PREFIX_TMPDIR=$(mktemp -d)
cd "$PREFIX_TMPDIR"
echo "$VERSION" > version

$TAR --append --file=$path --mtime=now --owner=0 --group=0 \
    --mode=g+w,a+rX --transform "s/^/$prefix\//" \
    version

cd -
rm -r "$PREFIX_TMPDIR"

# xz it
xz "$path"

# Output what we did, and the sha256 hash
echo "Tarball written to $path".xz
openssl dgst -sha256 "$path".xz
