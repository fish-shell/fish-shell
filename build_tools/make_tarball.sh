#!/bin/sh

rm -f /tmp/fish_fish.tar /tmp/fish_fish.tar.gz
if git archive --format=tar fish_fish | gzip - > /tmp/fish_fish.tar
then
	echo "Tarball written to /tmp/fish_fish.tar.gz"
else
	echo "Tarball could not be written"
fi
