#!/bin/sh

rm -f /tmp/fish_fish.tar /tmp/fish_fish.tar.gz
if git archive --format=tar fish_fish | gzip - > /tmp/fishfish.tar.gz
then
	echo "Tarball written to /tmp/fishfish.tar.gz"
else
	echo "Tarball could not be written"
fi
