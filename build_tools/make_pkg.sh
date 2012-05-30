#!/bin/sh -x

rm -rf /tmp/fish_pkg
mkdir -p /tmp/fish_pkg/
if make DESTDIR=/tmp/fish_pkg install
then
	echo "Root written to /tmp/fish_pkg/"
	if /Developer/usr/bin/packagemaker --doc ./build_tools/fish_shell.pmdoc --out ~/fish_built/fishfish.pkg
	then
		echo "Package written to ~/fish_built/fishfish.pkg"
	else
		echo "Package could not be written"
	fi
	
else
	echo "Root could not be written"
fi
