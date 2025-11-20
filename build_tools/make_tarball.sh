#!/bin/sh

# Script to generate a tarball
# We use git to output a tree. But we also want to build the user documentation
# and put that in the tarball, so that nobody needs to have sphinx installed
# to build it.
# Outputs to $FISH_ARTEFACT_PATH or ~/fish_built by default

# Exit on error
set -e

# We will generate a tarball with a prefix "fish-VERSION"
# git can do that automatically for us via git-archive
# but to get the documentation in, we need to make a symlink called "fish-VERSION"
# and tar from that, so that the documentation gets the right prefix

# Use Ninja if available, as it automatically parallelises
BUILD_TOOL="make"
BUILD_GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null; then
    BUILD_TOOL="ninja"
    BUILD_GENERATOR="Ninja"
fi

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
tag_creation_date=$(
    # If not dirty (i.e. we're building an immutable tag), pin the build date.
    if [ "$VERSION" = "$(git describe)" ]; then
        git log --format=%ad '--date=format:%b %d, %Y' -1
    fi
)

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
cmake -G "$BUILD_GENERATOR" -DCMAKE_BUILD_TYPE=Debug "$wd"
mkdir $PWD/user_doc/src
FISH_SPHINX_BUILD_DATE=$tag_creation_date \
$BUILD_TOOL doc

TAR_APPEND="$TAR --append --file=$path --mtime=now --owner=0 --group=0 \
    --mode=g+w,a+rX --transform s/^/$prefix\//"
$TAR_APPEND --no-recursion user_doc
$TAR_APPEND user_doc/html user_doc/man
$TAR_APPEND version

cd -
rm -r "$PREFIX_TMPDIR"

# xz it
xz "$path"

# Output what we did, and the sha256 hash
echo "Tarball written to $path".xz
openssl dgst -sha256 "$path".xz
