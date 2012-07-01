#!/bin/sh

rm -f ~/fish_built/fishfish.tar.gz
if git archive --format=tar --prefix=fishfish/ master | gzip - > ~/fish_built/fishfish.tar.gz
then
	echo "Tarball written to ~/fish_built/fishfish.tar.gz"
else
	echo "Tarball could not be written"
fi
