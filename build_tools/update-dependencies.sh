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

update_gh_action() {
    repo=$1
    version=$(curl -fsS "https://api.github.com/repos/$repo/releases/latest" | jq -r .tag_name)
    [ -n "$version" ]
    tag_oid=$(git ls-remote "https://github.com/$repo.git" "refs/tags/$version" | cut -f1)
    [ -n "$tag_oid" ]
    find .github/workflows -name '*.yml' -type f -exec \
        sed -i "s|uses: $repo@\S\+\( \+#.*\)\?|\
uses: $repo@$tag_oid # $version, build_tools/update-dependencies.sh|g" {} +
}
update_gh_action actions/checkout
update_gh_action actions/github-script
update_gh_action actions/upload-artifact
update_gh_action actions/download-artifact
update_gh_action docker/login-action
update_gh_action docker/build-push-action
update_gh_action docker/metadata-action
update_gh_action EmbarkStudios/cargo-deny-action
update_gh_action dessant/lock-threads
update_gh_action softprops/action-gh-release
update_gh_action msys2/setup-msys2

updatecli "${@:-apply}"

# Python version constraints may have changed.
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
