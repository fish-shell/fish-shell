#!/bin/sh

set -ex

RUSTFLAGS='-D warnings'; export RUSTFLAGS
RUSTDOCFLAGS='-D warnings'; export RUSTDOCFLAGS

repo_root="$(dirname "$0")/.."
build_dir="$repo_root/target/debug"

PATH="$build_dir:$PATH" "$repo_root/build_tools/style.fish" --all --check

cargo build --workspace --all-targets
cargo clippy --workspace --all-targets
cargo test --no-default-features --workspace --all-targets
cargo test --doc --workspace
cargo doc --workspace

# TODO: parallelize
"$repo_root/tests/test_driver.py" "$build_dir"
