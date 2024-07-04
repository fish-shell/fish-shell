#!/usr/bin/env bash

# Code sign executables or packages for macOS using rcodesign.

set -e

die() { echo "$*" 1>&2; exit 1; }

usage() {
  die "Usage: $0 -f <p12 file> -p <p12 password> [-e <entitlements file>] <executable1> [<executable2> ...]"
}

while getopts "i:f:p:e:" opt; do
  case $opt in
    f) P12_FILE="$OPTARG";;
    p) P12_PASSWORD="$OPTARG";;
    e) ENTITLEMENTS_FILE="$OPTARG";;
    \?) usage;;
  esac
done

shift $((OPTIND -1))

if [ -z "$P12_FILE" ] || [ -z "$P12_PASSWORD" ]; then
  usage
fi

if [ $# -lt 1 ]; then
  usage
fi

if ! command -v rcodesign &> /dev/null; then
  die "rcodesign is not installed. Please install it: cargo install apple-codesign"
fi

for EXECUTABLE in "$@"; do
    ARGS=(
        --p12-file "$P12_FILE"
        --p12-password "$P12_PASSWORD"
        --code-signature-flags runtime
    )

    if [ -n "$ENTITLEMENTS_FILE" ]; then
        ARGS+=(--entitlements-xml-file "$ENTITLEMENTS_FILE")
    fi

    rcodesign sign "${ARGS[@]}" $EXECUTABLE
done
