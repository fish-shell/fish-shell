#!/bin/sh -x

rm -rf /tmp/fish_pkg
mkdir -p /tmp/fish_pkg/
if make install prefix=/tmp/fish_pkg/usr/local/
then
	echo "Root written to /tmp/fish_pkg/"
	if pkgbuild --identifier com.ridiculousfish.fish-shell --scripts build_tools/osx_package_scripts/ --root /tmp/fish_pkg/ ~/fish_installer.pkg
	then
		echo "Package written to ~/fish_installer.pkg"
	else
		echo "Package could not be written"
	fi
	
else
	echo "Root could not be written"
fi
