#!/bin/sh

# Script to produce an OS X installer .pkg and .app(.zip)

VERSION=`git describe --always --dirty 2>/dev/null`
if test -z "$VERSION" ; then
  echo "Could not get version from git"
  VERSION=`sed -E -n 's/^.*PACKAGE_VERSION "([0-9a-z.\-]+)"/\1/p' osx/config.h`
  if test -z "$VERSION"; then
    echo "Could not get version from osx/config.h"
    exit 1
  fi
fi

echo "Version is $VERSION"

set -x

make distclean

#Exit on error
set -e

PKGDIR=`mktemp -d`

OUTPUT_PATH=${FISH_ARTEFACT_PATH:-~/fish_built}

mkdir -p $PKGDIR/root $PKGDIR/intermediates $PKGDIR/dst
xcodebuild install -scheme install_tree -configuration Release DSTROOT=$PKGDIR/root/
pkgbuild --scripts build_tools/osx_package_scripts --root $PKGDIR/root/ --identifier 'com.ridiculousfish.fish-shell-pkg' --version "$VERSION"  $PKGDIR/intermediates/fish.pkg

productbuild  --package-path $PKGDIR/intermediates --distribution build_tools/osx_distribution.xml --resources build_tools/osx_package_resources/ $OUTPUT_PATH/fish-$VERSION.pkg


# Make the app
xcodebuild -scheme fish.app -configuration Release DSTROOT=/tmp/fish_app/ SYMROOT=DerivedData/fish/Build/Products

cd DerivedData/fish/Build/Products/Release/
zip -r $OUTPUT_PATH/fish-$VERSION.app.zip fish.app

rm -r $PKGDIR
