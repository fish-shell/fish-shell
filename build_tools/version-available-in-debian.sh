#!/bin/bash

set -euo pipefail

channel=$1 # e.g. stable, testing
package=$2 # e.g. rustc, sphinx

codename=$(
    curl -fsS https://ftp.debian.org/debian/dists/"${channel}"/Release |
    grep '^Codename:' | cut -d' ' -f2)
curl -fsS https://sources.debian.org/api/src/"${package}"/ |
    jq -r --arg codename "${codename}" '
        .versions[] | select(.suites[] == $codename) | .version' |
    sed 's/^\([0-9]\+\.[0-9]\+\).*/\1/' |
    sort --version-sort |
    tail -1
