#!/bin/sh
# Originally from the git sources (GIT-VERSION-GEN)
# Presumably (C) Junio C Hamano <junkio@cox.net>
# Reused under GPL v2.0
# Modified for fish by David Adam <zanchey@ucc.gu.uwa.edu.au>

FBVF=FISH-BUILD-VERSION-FILE
DEF_VER=unknown

# First see if there is a version file (included in release tarballs),
# then try git-describe, then default.
if test -f version
then
	VN=$(cat version) || VN="$DEF_VER"
elif ! VN=$(git describe --always --dirty 2>/dev/null); then
	VN="$DEF_VER"
fi

if test -r $FBVF
then
	VC=$(sed -e 's/^FISH_BUILD_VERSION = //' <$FBVF)
else
	VC=unset
fi
test "$VN" = "$VC" || {
	echo >&2 "FISH_BUILD_VERSION = $VN"
	echo "FISH_BUILD_VERSION = $VN" >$FBVF
}
