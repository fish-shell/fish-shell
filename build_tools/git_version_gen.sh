#!/usr/bin/env sh
# Originally from the git sources (GIT-VERSION-GEN)
# Presumably (C) Junio C Hamano <junkio@cox.net>
# Reused under GPL v2.0
# Modified for fish by David Adam <zanchey@ucc.gu.uwa.edu.au>

set -e

# Find the fish directory as two levels up from script directory.
FISH_BASE_DIR="$( cd "$( dirname "$( dirname "$0" )" )" && pwd )"
DEF_VER=unknown

# First see if there is a version file (included in release tarballs),
# then try git-describe, then default.
if test -f version
then
	VN=$(cat version) || VN="$DEF_VER"
elif ! VN=$(git -C "$FISH_BASE_DIR" describe --always --dirty 2>/dev/null); then
	VN="$DEF_VER"
fi

# If the first param is --stdout, then output to stdout and exit.
if test "$1" = '--stdout'
then
	echo $VN
    exit 0
fi

# Set the output directory as either the first param or cwd.
test -n "$1" && OUTPUT_DIR=$1/ || OUTPUT_DIR=
FBVF=${OUTPUT_DIR}FISH-BUILD-VERSION-FILE

if test -r $FBVF
then
	VC=$(grep -v '^#' $FBVF | tr -d '"' | sed -e 's/^FISH_BUILD_VERSION=//')
else
	VC="unset"
fi

# Maybe output the FBVF
# It looks like FISH_BUILD_VERSION="2.7.1-621-ga2f065e6"
test "$VN" = "$VC" || {
	echo >&2 "FISH_BUILD_VERSION=$VN"
	echo "FISH_BUILD_VERSION=\"$VN\"" >${FBVF}
}

# Output the fish-build-version-witness.txt
# See https://cmake.org/cmake/help/v3.4/policy/CMP0058.html
date +%s > ${OUTPUT_DIR}fish-build-version-witness.txt
