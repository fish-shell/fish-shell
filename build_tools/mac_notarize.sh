#!/bin/sh

# Helper to notarize an .app.zip or .pkg file.

set -e

die() { echo "$*" 1>&2 ; exit 1; }


test "$#" -ge 1 || die "No paths specified."

for INPUT in "$@"; do
    echo "Processing $INPUT"
    test -f "$INPUT" || die "Not a file: $INPUT"
    ext="${INPUT##*.}"
    { test "$ext" = "zip" || test "$ext" = "pkg"; } || die "Unrecognized extension: $ext"

    xcrun notarytool submit "$INPUT" --keychain-profile AC_PASSWORD --wait

    if test "$ext" = "zip"; then
        TMPDIR=$(mktemp -d)
        echo "Extracting to $TMPDIR"
        unzip -q "$INPUT" -d "$TMPDIR"
        STAPLE_TARGET=$(echo "$TMPDIR"/*)
    else
        STAPLE_TARGET="$INPUT"
    fi
    echo "Stapling $STAPLE_TARGET"
    xcrun stapler staple "$STAPLE_TARGET"

    if test "$ext" = "zip"; then
        # Zip it back up.
        INPUT_FULL=$(realpath "$INPUT")
        rm -f "$INPUT"
        cd "$(dirname "$STAPLE_TARGET")"
        zip -r -q "$INPUT_FULL" "$(basename "$STAPLE_TARGET")"
    fi
    echo "Processed $INPUT"

    if test "$ext" = "zip"; then
        spctl -a -v "$STAPLE_TARGET"
    fi
done
