#!/bin/sh

VERSION=`sed -E -n 's/^.*PACKAGE_VERSION "([0-9.]+)"/\1/p' osx/config.h`
if test -z "$VERSION" ; then
  echo "Could not get version from osx/config.h"
  exit 1
fi

echo "Version is $VERSION"

set -x

make distclean
rm -Rf /tmp/fish_pkg

#Exit on error
set -e

mkdir -p /tmp/fish_pkg/root /tmp/fish_pkg/intermediates /tmp/fish_pkg/dst
xcodebuild install -scheme install_tree -configuration Release DSTROOT=/tmp/fish_pkg/root/
pkgbuild --scripts build_tools/osx_package_scripts --root /tmp/fish_pkg/root/ --identifier 'com.ridiculousfish.fish-shell-pkg' --version "$VERSION"  /tmp/fish_pkg/intermediates/fish.pkg

productbuild  --package-path /tmp/fish_pkg/intermediates --distribution build_tools/osx_distribution.xml --resources build_tools/osx_package_resources/ ~/fish_built/fish.pkg


# Make the app
xcodebuild -scheme fish.app -configuration Release DSTROOT=/tmp/fish_app/
rm -f ~/fish_built/fish.app.zip
cd DerivedData/fish/Build/Products/Release/
zip -r ~/fish_built/fish.app.zip fish.app
