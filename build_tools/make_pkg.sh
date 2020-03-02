#!/usr/bin/env bash

# Script to produce an OS X installer .pkg and .app(.zip)

VERSION=$(git describe --always --dirty 2>/dev/null)
if test -z "$VERSION" ; then
  echo "Could not get version from git"
  if test -f version; then
    VERSION=$(cat version)
  fi
fi

echo "Version is $VERSION"

set -x

#Exit on error
set -e

# Respect MAC_CODESIGN_ID and MAC_PRODUCTSIGN_ID, or default for ad-hoc.
# Note the :- means "or default" and the following - is the value.
MAC_CODESIGN_ID=${MAC_CODESIGN_ID:--}
MAC_PRODUCTSIGN_ID=${MAC_PRODUCTSIGN_ID:--}

PKGDIR=$(mktemp -d)

SRC_DIR=$PWD
OUTPUT_PATH=${FISH_ARTEFACT_PATH:-~/fish_built}

mkdir -p "$PKGDIR/build" "$PKGDIR/root" "$PKGDIR/intermediates" "$PKGDIR/dst"
{ cd "$PKGDIR/build" && cmake -DMAC_INJECT_GET_TASK_ALLOW=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo -DMAC_CODESIGN_ID="${MAC_CODESIGN_ID}" "$SRC_DIR" && make -j 12 && env DESTDIR="$PKGDIR/root/" make install; }
pkgbuild --scripts "$SRC_DIR/build_tools/osx_package_scripts" --root "$PKGDIR/root/" --identifier 'com.ridiculousfish.fish-shell-pkg' --version "$VERSION" "$PKGDIR/intermediates/fish.pkg"
productbuild  --package-path "$PKGDIR/intermediates" --distribution "$SRC_DIR/build_tools/osx_distribution.xml" --resources "$SRC_DIR/build_tools/osx_package_resources/" "$OUTPUT_PATH/fish-$VERSION.pkg"
productsign --sign "${MAC_PRODUCTSIGN_ID}" "$OUTPUT_PATH/fish-$VERSION.pkg" "$OUTPUT_PATH/fish-$VERSION-signed.pkg" && mv "$OUTPUT_PATH/fish-$VERSION-signed.pkg" "$OUTPUT_PATH/fish-$VERSION.pkg"

# Make the app
{ cd "$PKGDIR/build" && make signed_fish_macapp && zip -r "$OUTPUT_PATH/fish-$VERSION.app.zip" fish.app; }

rm -r "$PKGDIR"
