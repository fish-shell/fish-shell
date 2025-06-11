#!/bin/sh

set -ex

cleanup () {
  if [ -n "$template_file" ] && [ -e "$template_file" ]; then
    rm "$template_file"
  fi
}

trap cleanup EXIT INT TERM HUP

RUSTFLAGS='-D warnings'; export RUSTFLAGS
RUSTDOCFLAGS='-D warnings'; export RUSTDOCFLAGS

repo_root="$(dirname "$0")/.."
build_dir="$repo_root/target/debug"

template_file=$(mktemp)
FISH_GETTEXT_EXTRACTION_FILE=$template_file cargo build --workspace --all-targets
PATH="$build_dir:$PATH" "$repo_root/build_tools/style.fish" --all --check
cargo clippy --workspace --all-targets
cargo test --no-default-features --workspace --all-targets
cargo test --doc --workspace
cargo doc --workspace
FISH_GETTEXT_EXTRACTION_FILE=$template_file "$repo_root/tests/test_driver.py" "$build_dir"
