#!/bin/sh

path=~/fish_built/fishfish-2.0.tar.gz
rm -f "$path"
if git archive --format=tar --prefix=fishfish/ master | gzip - > "$path"
then
	echo "Tarball written to $path"
	openssl sha1 "$path"
else
	echo "Tarball could not be written"
fi
