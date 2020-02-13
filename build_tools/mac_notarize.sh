#!/usr/bin/env bash

# Helper to notarize an .app.zip or .pkg file.
# Based on https://www.logcg.com/en/archives/3222.html

set -e

die() { echo "$*" 1>&2 ; exit 1; }

check_status() {
    echo "STATUS" $1
}

get_req_uuid() {
    RESPONSE=$(</dev/stdin)
    if echo "$RESPONSE" | egrep -q "RequestUUID"; then
        echo "$RESPONSE" | egrep RequestUUID | awk '{print $3'}
    elif echo "$RESPONSE" | egrep -q "The upload ID is "; then
        echo "$RESPONSE" | egrep -p "The upload ID is [-a-z0-9]+" | awk '{print $5}'
    else
        die "Could not get Request UUID"
    fi
}

INPUT=$1
AC_USER=$2

test -z "$AC_USER" && die "AC_USER not specified as second param"
test -z "$INPUT" && die "No path specified"
test -f "$INPUT" || die "Not a file: $INPUT"

ext="${INPUT##*.}"
(test "$ext" = "zip" || test "$ext" = "pkg") || die "Unrecognized extension: $ext"

LOGFILE=$(mktemp -t mac_notarize_log)
AC_PASS="@keychain:AC_PASSWORD"
echo "Logs at $LOGFILE"

NOTARIZE_UUID=$(xcrun altool --notarize-app \
                --primary-bundle-id "com.ridiculousfish.fish-shell" \
                --username "$AC_USER" \
                --password "$AC_PASS" \
                --file "$INPUT" 2>&1 |
                    tee -a "$LOGFILE" |
                    get_req_uuid)

test -z "$NOTARIZE_UUID" && cat "$LOGFILE" && die "Could not get RequestUUID"
echo "RequestUUID: $NOTARIZE_UUID"

success=0
for i in $(seq 20); do
    echo "Checking progress..."
    PROGRESS=$(xcrun altool --notarization-info "${NOTARIZE_UUID}" \
                -u "$AC_USER" \
                -p "$AC_PASS" 2>&1 |
                    tee -a "$LOGFILE")
    echo "${PROGRESS}" | tail -n 1

    if [ $? -ne 0 ] || [[ "${PROGRESS}" =~ "Invalid" ]] ; then
        echo "Error with notarization. Exiting"
        break
    fi

    if ! [[ "${PROGRESS}" =~ "in progress" ]]; then
        success=1
        break
    else
        echo "Not completed yet. Sleeping for 30 seconds."
    fi
    sleep 30
done

if [ $success -eq 1 ] ; then
    if test "$ext" = "zip"; then
        TMPDIR=$(mktemp -d)
        echo "Extracting to $TMPDIR"
        unzip -q "$INPUT" -d "$TMPDIR"
        # Force glob expansion.
        STAPLE_TARGET="$TMPDIR"/*
        STAPLE_TARGET=$(echo $STAPLE_TARGET)
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
        zip -r -q "$INPUT_FULL" $(basename "$STAPLE_TARGET")
    fi
fi
echo "Processed $INPUT"

if test "$ext" = "zip"; then
    spctl -a -v "$STAPLE_TARGET"
fi
