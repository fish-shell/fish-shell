#!/bin/sh

# Script to generate the dput.cf for a set of Ubuntu series, prints the filename
# Arguments are the PPA followed by the series names

set -e

outfile=$(mktemp --tmpdir dput.XXXXX.cf)

[ $# -lt 2 ] &&
    echo "$0: at least two arguments (a PPA and at least one series) are required" >&2 &&
    exit 1

ppa=$1
shift

for series in "$@"; do
    cat >> "$outfile" <<EOF
[fish-$ppa-$series]
fqdn = ppa.launchpad.net
method = ftp
login = anonymous
incoming = ~fish-shell/$ppa/ubuntu/$series

EOF
done

echo "$outfile"
