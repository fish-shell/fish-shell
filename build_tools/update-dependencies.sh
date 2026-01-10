#!/bin/sh

set -ex

command -v curl
command -v gcloud
command -v jq
command -v rustup
command -v updatecli
command -v uv
sort --version-sort </dev/null

# TODO This is copied from .github/actions/install-sphinx/action.yml
uv lock --check --exclude-newer="$(awk -F'"' <uv.lock '/^exclude-newer[[:space:]]*=/ {print $2}')"

updatecli "${@:-apply}"

uv lock # Python version constraints may have changed.
uv lock --upgrade --exclude-newer="$(date --date='7 days ago' --iso-8601)"

from_gh() {
    repo=$1
    path=$2
    destination=$3
    contents=$(curl -fsS https://raw.githubusercontent.com/"${repo}"/refs/heads/master/"${path}")
    printf '%s\n' >"$destination" "$contents"
}

from_gh ridiculousfish/widecharwidth widechar_width.rs crates/widecharwidth/src/widechar_width.rs
from_gh ridiculousfish/littlecheck littlecheck/littlecheck.py tests/littlecheck.py
from_gh catppuccin/fish 'themes/Catppuccin Frappe.theme' share/themes/catppuccin-frappe.theme
from_gh catppuccin/fish 'themes/Catppuccin Macchiato.theme' share/themes/catppuccin-macchiato.theme
from_gh catppuccin/fish 'themes/Catppuccin Mocha.theme' share/themes/catppuccin-mocha.theme

# Update Cargo.lock
cargo update
# Update Cargo.toml and Cargo.lock
cargo +nightly -Zunstable-options update --breaking
