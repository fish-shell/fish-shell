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
VERSION=`git describe --dirty 2>/dev/null`
prefix="$prefix-$VERSION"

# The path where we will output the tar file
path=~/fish_built/$prefix.tar

# Clean up stuff we've written before
rm -f "$path" "$path".gz

# git starts the archive
git archive --format=tar --prefix="$prefix"/ HEAD > "$path"

# tarball out the documentation, generate a configure script and version file
# Don't use autoreconf since it invokes commands that may not be installed, like aclocal
# Don't run autoheader since configure.ac runs it. autoconf is enough.
autoconf
./configure --with-doxygen
make doc share/man
echo $VERSION > version
cd /tmp
rm -f "$prefix"
ln -s "$wd" "$prefix"
TAR_APPEND="gnutar --append --file=$path --mtime=now --owner=0 --group=0 --mode=g+w,a+rX"
$TAR_APPEND --no-recursion "$prefix"/user_doc
$TAR_APPEND "$prefix"/user_doc/html "$prefix"/share/man
$TAR_APPEND "$prefix"/version
$TAR_APPEND "$prefix"/configure "$prefix"/config.h.in
rm -f "$prefix"/version
rm -f "$prefix"

# gzip it
gzip "$path"

# Output what we did, and the sha1 hash
echo "Tarball written to $path".gz
openssl sha1 "$path".gz
