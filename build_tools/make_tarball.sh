#!/bin/sh

# Script to generate a tarball
# We use git to output a tree. But we also want to build the user documentation
# and put that in the tarball, so that nobody needs to have doxygen installed
# to build it.

# Exit on error
set -e

# We wil generate a tarball with a prefix "fish"
# git can do that automatically for us via git-archive
# but to get the documentation in, we need to make a symlink called "fish"
# and tar from that, so that the documentation gets the right prefix

# Get the current directory, which we'll use for symlinks
wd="$PWD"

# The name of the prefix, which is the directory that you get when you untar
prefix="fish"

# Get the version from git-describe
VERSION=`git describe --tags --dirty 2>/dev/null`
prefix="$prefix-$VERSION"

# The path where we will output the tar file
path=~/fish_built/$prefix.tar

# Clean up stuff we've written before
rm -f "$path" "$path".gz

# git starts the archive
git archive --format=tar --prefix="$prefix"/ master > "$path"

# tarball out the documentation
make user_doc
make share/man
echo $VERSION > version
cd /tmp
rm -f "$prefix"
ln -s "$wd" "$prefix"
gnutar --append --file="$path" "$prefix"/user_doc/html
gnutar --append --file="$path" "$prefix"/share/man
gnutar --append --file="$path" "$prefix"/version
rm -f "$prefix"/version
rm -f "$prefix"

# gzip it
gzip "$path"

# Output what we did, and the sha1 hash
echo "Tarball written to $path".gz
openssl sha1 "$path".gz
