#!/bin/sh

# Terminate on error
set -e

sudo rm -Rf /tmp/fishfish
mkdir /tmp/fishfish
git archive --format=tar fish_fish | tar -x -C /tmp/fishfish
mkdir /tmp/fishfish/doc-pak
cp README INSTALL CHANGELOG release_notes.html /tmp/fishfish/doc-pak/
cp build_tools/description-pak /tmp/fishfish/
cd /tmp/fishfish
autoconf
./configure
make -j 3
sudo checkinstall --default --pakdir ~/fish_built/ --pkgversion 0.9 make install
mv ~/fish_built/fishfish_0.9-1_i386.deb ~/fish_built/fishfish_0.9_i386.deb

