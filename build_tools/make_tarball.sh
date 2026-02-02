#!/bin/sh

# Script to generate a tarball
# Outputs to $FISH_ARTEFACT_PATH or ~/fish_built by default

set -e

# Get the version
VERSION=$(build_tools/git_version_gen.sh)

prefix=fish-$VERSION
path=${FISH_ARTEFACT_PATH:-~/fish_built}/$prefix.tar.xz

tmpdir=$(mktemp -d)
manifest=$tmpdir/Cargo.toml
lockfile=$tmpdir/Cargo.lock

sed "s/^version = \".*\"\$/version = \"$VERSION\"/g" Cargo.toml \
 >"$manifest"
awk -v version=$VERSION '
    /^name = "fish"$/ { ok=1 }
    ok == 1 && /^version = ".*"$/ {
        ok = 2;
        $0 = "version = \"" version "\"";
    }
    {print}
	' \
    Cargo.lock >"$lockfile"

for ext in toml lock; do
    if cmp Cargo.$ext "$tmpdir/Cargo.$ext" >/dev/null; then
        echo >&2 "failed to update Cargo.$ext version?"
        exit 1
    fi
done

git archive \
    --prefix="$prefix/" \
    --add-virtual-file="$prefix/Cargo.toml:$(cat "$manifest")" \
    --add-virtual-file="$prefix/Cargo.lock:$(cat "$lockfile")" \
    HEAD |
    xz >"$path"

rm "$manifest"
rm "$lockfile"
rmdir "$tmpdir"

# Output what we did, and the sha256 hash
echo "Tarball written to $path"
openssl dgst -sha256 "$path"
