#!/bin/sh -x

make distclean
rm -rf /tmp/fish_pkg
mkdir -p /tmp/fish_pkg/

# Make sure what we build can run on SnowLeopard
export OSX_SDK="/Developer/SDKs/MacOSX10.6.sdk"
export MACOSX_DEPLOYMENT_TARGET="10.6"
export CC="clang -isysroot $OSX_SDK -mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"
export CCX="clang++ -isysroot $OSX_SDK -mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"
export CFLAGS="$CFLAGS -isysroot $OSX_SDK -mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"
export CXXFLAGS="$CXXFLAGS -isysroot $OSX_SDK -mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"
export LDFLAGS="$LDFLAGS -isysroot $OSX_SDK -mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"
autoconf
./configure --without-xsel

# Actually build it now
if make -j 4 DESTDIR=/tmp/fish_pkg install
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
