#!/bin/sh

set -ex

command -v curl
command -v gcloud
command -v jq
command -v rustup
command -v updatecli
command -v uv
sort --version-sort </dev/null

uv lock --check

updatecli "${@:-apply}"

uv lock # Python version constraints may have changed.
uv lock --upgrade --exclude-newer="$(date --date='7 days ago' --iso-8601)"

from_gh() {
    repo=$1
    path=$2
    out_dir=$3
    contents=$(curl -fsS https://raw.githubusercontent.com/"${repo}"/refs/heads/master/"${path}")
    printf '%s\n' >"$out_dir/$(basename "$path")" "$contents"
}
from_gh ridiculousfish/widecharwidth widechar_width.rs crates/widecharwidth/src/
from_gh ridiculousfish/littlecheck littlecheck/littlecheck.py tests/

# Update Cargo.lock
cargo update
# Update Cargo.toml and Cargo.lock
cargo +nightly -Zunstable-options update --breaking
