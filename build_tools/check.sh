#!/bin/sh

{
set -ex

lint=true
if [ "$FISH_CHECK_LINT" = false ]; then
    lint=false
fi

check_dependency_versions=false
if [ "${FISH_CHECK_DEPENDENCY_VERSIONS:-false}" != false ]; then
    check_dependency_versions=true
fi

if $check_dependency_versions; then
    command -v curl
    command -v jq
    command -v rustup
    command -v uv
    sort --version-sort </dev/null
    # To match existing behavior, only check Rust/dockerfiles for now.
    # TODO: remove this from this script.
    updatecli diff --config=updatecli.d/docker.yml --config=updatecli.d/rust.yml
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
    if [ -n "$fluent_id_file" ] && [ -e "$fluent_id_file" ]; then
        rm "$fluent_id_file"
    fi
    if [ -n "$gettext_template_file" ] && [ -e "$gettext_template_file" ]; then
        rm "$gettext_template_file"
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

if [ -n "$FISH_TEST_MAX_CONCURRENCY" ]; then
    export RUST_TEST_THREADS="$FISH_TEST_MAX_CONCURRENCY"
    export CARGO_BUILD_JOBS="$FISH_TEST_MAX_CONCURRENCY"
fi

fluent_id_file=$(mktemp)
gettext_template_file=$(mktemp)
(
    export FISH_GETTEXT_EXTRACTION_FILE="$gettext_template_file"
    # shellcheck disable=2030
    export FISH_FLUENT_ID_FILE="$fluent_id_file"
    cargo build --workspace --all-targets --features=fluent-extract,gettext-extract
)
if $lint; then
    PATH="$build_dir:$PATH" "$workspace_root/build_tools/style.fish" --all --check
    for features in "" --no-default-features; do
        cargo clippy --workspace --all-targets $features
    done
fi
(
    # shellcheck disable=2031
    export FISH_FLUENT_ID_FILE="$fluent_id_file"
    cargo test --no-default-features --workspace --all-targets
)
cargo test --doc --workspace
if $lint; then
    cargo doc --workspace
fi
FISH_GETTEXT_EXTRACTION_FILE=$gettext_template_file "$workspace_root/tests/test_driver.py" "$build_dir"

exit
}
