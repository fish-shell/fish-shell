#!/bin/sh

# This script takes a source tarball (from build_tools/make_tarball.sh) and a vendor tarball (from
# build_tools/make_vendor_tarball.sh, generated if not present), and produces:
# * Appropriately-named symlinks to look like a Debian package
# * Debian .changes and .dsc files with plain names ($version-1) and supported Ubuntu prefixes
#   ($version-1~somedistro)
# * An RPM spec file
# By default, input and output files go in ~/fish_built, but this can be controlled with the
# FISH_ARTEFACT_PATH environment variable.

{

set -e

version=$1
[ -n "$version" ] || { echo "Version number required as argument" >&2; exit 1; }


[ -n "$DEB_SIGN_KEYID$DEB_SIGN_KEYFILE" ] ||
    echo "Warning: neither DEB_SIGN_KEYID or DEB_SIGN_KEYFILE environment variables are set; you
will need a signing key for the author of the most recent debian/changelog entry." >&2

workpath=${FISH_ARTEFACT_PATH:-~/fish_built}
source_tarball="$workpath"/fish-"$version".tar.xz
vendor_tarball="$workpath"/fish-"$version"-vendor.tar.xz

[ -e "$source_tarball" ] || { echo "Missing source tarball, expected at $source_tarball" >&2; exit 1; }
cd "$workpath"

# Unpack the sources
tar xf "$source_tarball"
sourcepath="$workpath"/fish-"$version"

# Generate the vendor tarball if it is not already present
[ -e "$vendor_tarball" ] || (cd "$sourcepath"; build_tools/make_vendor_tarball.sh;)

# This step requires network access, so do it early in case it fails
# sh has no real array support
ubuntu_versions=$(uv run --script "$sourcepath"/build_tools/supported_ubuntu_versions.py)

# Write the specfile
[ -e "$workpath"/fish.spec ] && { echo "Cowardly refusing to overwite an existing fish.spec" >&2;
exit 1; }
rpmversion=$(echo "$version" |sed -e 's/-/+/' -e 's/-/./g')
sed -e "s/@version@/$version/g" -e "s/@rpmversion@/$rpmversion/g" \
    < "$sourcepath"/fish.spec.in > "$workpath"/fish.spec

# Make the symlinks for Debian
ln -s "$source_tarball" "$workpath"/fish_"$version".orig.tar.xz
ln -s "$vendor_tarball" "$workpath"/fish_"$version".orig-cargo-vendor.tar.xz

# Set up the Debian source tree
cd "$sourcepath"
mkdir cargo-vendor
tar -C cargo-vendor -x -f "$vendor_tarball"
cp -r contrib/debian debian

# The vendor tarball contains a new .cargo/config.toml, which has the
# vendoring overrides appended to it. dpkg-source will add this as a
# patch using the flags in debian/
cp cargo-vendor/.cargo/config.toml .cargo/config.toml

# Update the Debian changelog
# The release scripts do this for release builds - skip if it has already been done
if head -n1 debian/changelog | grep --invert-match --quiet --fixed-strings "$version"; then
    debchange --newversion "$version-1" --distribution unstable "Snapshot build"
fi

# Builds the "plain" Debian package
# debuild runs lintian, which takes ten minutes to run over the vendor directories
# just use dpkg-buildpackage directly
dpkg-buildpackage --build=source -d

# Build the Ubuntu packages
# deb-reversion does not work on source packages, so do the whole thing ourselves
for series in $ubuntu_versions; do
    sed -i -e "1 s/$version-1)/$version-1~$series)/" -e "1 s/unstable/$series/" debian/changelog
    dpkg-buildpackage --build=source -d
    sed -i -e "1 s/$version-1~$series)/$version-1)/" -e "1 s/$series/unstable/" debian/changelog
done

}
