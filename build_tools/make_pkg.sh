#!/bin/sh

# Script to produce an OS X installer .pkg and .app(.zip)

VERSION=`git describe --always --dirty 2>/dev/null`
if test -z "$VERSION" ; then
  echo "Could not get version from git"
fi

echo "Version is $VERSION"

set -x

#Exit on error
set -e

PKGDIR=`mktemp -d`

SRC_DIR=$PWD
OUTPUT_PATH=${FISH_ARTEFACT_PATH:-~/fish_built}

mkdir -p $PKGDIR/build $PKGDIR/root $PKGDIR/intermediates $PKGDIR/dst
( cd "$PKGDIR/build" && cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo "$SRC_DIR" && make -j 4 && env DESTDIR=$PKGDIR/root/ make install )
pkgbuild --scripts build_tools/osx_package_scripts --root $PKGDIR/root/ --identifier 'com.ridiculousfish.fish-shell-pkg' --version "$VERSION"  $PKGDIR/intermediates/fish.pkg

productbuild  --package-path $PKGDIR/intermediates --distribution build_tools/osx_distribution.xml --resources build_tools/osx_package_resources/ $OUTPUT_PATH/fish-$VERSION.pkg


# Make the app
( cd "$PKGDIR/build" && make fish_macapp && zip -r "$OUTPUT_PATH/fish-$VERSION.app.zip" fish.app )

rm -r $PKGDIR
