#!/bin/sh

{
set -ex

lint=true
if [ "$FISH_CHECK_LINT" = false ]; then
    lint=false
fi

cargo_args=$FISH_CHECK_CARGO_ARGS
target_triple=$FISH_CHECK_TARGET_TRIPLE
if [ -n "$target_triple" ]; then
    cargo_args="$cargo_args --target=$FISH_CHECK_TARGET_TRIPLE"
fi

cargo() {
    subcmd=$1
    shift
    # shellcheck disable=2086
    command cargo "$subcmd" $cargo_args "$@"
}

cleanup () {
    if [ -n "$template_file" ] && [ -e "$template_file" ]; then
        rm "$template_file"
    fi
}

trap cleanup EXIT INT TERM HUP

if $lint; then
    export RUSTFLAGS="--deny=warnings ${RUSTFLAGS}"
    export RUSTDOCFLAGS="--deny=warnings ${RUSTDOCFLAGS}"
fi

workspace_root="$(dirname "$0")/.."
target_dir=${CARGO_TARGET_DIR:-$workspace_root/target}
if [ -n "$target_triple" ]; then
    target_dir="$target_dir/$target_triple"
fi
# The directory containing the binaries produced by cargo/rustc.
# Currently, all builds are debug builds.
build_dir="$target_dir/debug"

template_file=$(mktemp)
FISH_GETTEXT_EXTRACTION_FILE=$template_file cargo build --workspace --all-targets --features=gettext-extract
if $lint; then
    PATH="$build_dir:$PATH" "$workspace_root/build_tools/style.fish" --all --check
    for features in "" --no-default-features; do
        cargo clippy --workspace --all-targets $features
    done
fi
cargo test --no-default-features --workspace --all-targets
cargo test --doc --workspace
if $lint; then
    cargo doc --workspace
fi
FISH_GETTEXT_EXTRACTION_FILE=$template_file "$workspace_root/tests/test_driver.py" "$build_dir"

exit
}
