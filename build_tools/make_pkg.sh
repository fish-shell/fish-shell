#!/bin/sh -x

make distclean
rm -Rf /tmp/fish_pkg
mkdir -p /tmp/fish_pkg/root /tmp/fish_pkg/intermediates /tmp/fish_pkg/dst

xcodebuild install -scheme install_tree -configuration Release DSTROOT=/tmp/fish_pkg/root/
pkgbuild --scripts build_tools/osx_package_scripts --root /tmp/fish_pkg/root/ --identifier 'com.ridiculousfish.fish-shell-pkg' /tmp/fish_pkg/intermediates/fish.pkg

productbuild  --package-path /tmp/fish_pkg/intermediates --distribution build_tools/osx_distribution.xml --resources build_tools/osx_package_resources/ ~/fish_built/fish.pkg
