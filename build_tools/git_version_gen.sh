#!/bin/sh
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
if test -f "$FISH_BASE_DIR"/version
then
    VN=$(cat "$FISH_BASE_DIR"/version) || VN="$DEF_VER"
elif VN=$(git -C "$FISH_BASE_DIR" describe --always --dirty 2>/dev/null); then
   :
else
    VN="$DEF_VER"
fi

# If the first param is --stdout, then output to stdout and exit.
if test "$1" = '--stdout'
then
    echo "$VN"
    exit 0
fi
