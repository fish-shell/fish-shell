#!/bin/sh
# Originally from the git sources (GIT-VERSION-GEN)
# Presumably (C) Junio C Hamano <junkio@cox.net>
# Reused under GPL v2.0
# Modified for fish by David Adam <zanchey@ucc.gu.uwa.edu.au>

set -e

# Find the fish directory as two levels up from script directory.
FISH_BASE_DIR="$( cd "$( dirname "$( dirname "$0" )" )" && pwd )"
DEF_VER=unknown
git_permission_failed=0

# First see if there is a version file (included in release tarballs),
# then try git-describe, then default.
if test -f "$FISH_BASE_DIR"/version
then
	VN=$(cat "$FISH_BASE_DIR"/version) || VN="$DEF_VER"
else
    if VN=$(git -C "$FISH_BASE_DIR" describe --always --dirty 2>/dev/null); then
       :
    else
        if test $? = 128; then
            # Current git versions return status 128
            # when run in a repo owned by another user.
            # Even for describe and everything.
            # This occurs for `sudo make install`.
            git_permission_failed=1
        fi
	    VN="$DEF_VER"
    fi
fi

# If the first param is --stdout, then output to stdout and exit.
if test "$1" = '--stdout'
then
	echo $VN
    exit 0
fi

# Set the output directory as either the first param or cwd.
test -n "$1" && OUTPUT_DIR=$1/ || OUTPUT_DIR=
FBVF="${OUTPUT_DIR}FISH-BUILD-VERSION-FILE"

if test "$VN" = unknown && test -r "$FBVF" && test "$git_permission_failed" = 1
then
    # HACK: Git failed, so we keep the current version file.
    # This helps in case you built fish as a normal user
    # and then try to `sudo make install` it.
    date +%s > "${OUTPUT_DIR}"fish-build-version-witness.txt
    exit 0
fi

if test -r "$FBVF"
then
	VC=$(cat "$FBVF")
else
	VC="unset"
fi

# Maybe output the FBVF
# It looks like "2.7.1-621-ga2f065e6"
test "$VN" = "$VC" || {
	echo >&2 "$VN"
	echo "$VN" >"$FBVF"
}

# Output the fish-build-version-witness.txt
# See https://cmake.org/cmake/help/v3.4/policy/CMP0058.html
date +%s > "${OUTPUT_DIR}"fish-build-version-witness.txt
