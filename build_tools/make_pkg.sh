#!/bin/sh -x

rm -rf /tmp/fish_pkg
mkdir -p /tmp/fish_pkg/
if make install prefix=/tmp/fish_pkg/usr/local/
then
	echo "Root written to /tmp/fish_pkg/"
	if /Developer/usr/bin/packagemaker --doc ./build_tools/fish_shell.pmdoc --out ~/fishfish.pkg
	then
		echo "Package written to ~/fishfish.pkg"
	else
		echo "Package could not be written"
	fi
	
else
	echo "Root could not be written"
fi
