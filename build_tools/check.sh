#!/bin/sh

{
set -ex

workspace_root="$(dirname "$0")/.."

fish_check_sanitize=false
while [ $# -gt 0 ]; do
    case $1 in
        --sanitize)
            fish_check_sanitize=true
            shift
            ;;
        *)
            echo "Unexpected argument: $1"
            exit 1
            ;;
    esac
done

if [ "$fish_check_sanitize" = true ]; then
    . "$workspace_root/build_tools/set_asan_vars.sh"

    # Variables used at build time

    export FISH_CHECK_RUST_TOOLCHAIN="${FISH_CHECK_RUST_TOOLCHAIN:-nightly}"
    export FISH_CHECK_CARGO_ARGS="-Zbuild-std $FISH_CHECK_CARGO_ARGS"
    # Build fails if this is not set.
    if [ -z "$FISH_CHECK_TARGET_TRIPLE" ]; then
        FISH_CHECK_TARGET_TRIPLE="$(rustc --print host-tuple)"
    fi
    export FISH_CHECK_TARGET_TRIPLE


    # Variables used at runtime

    if [ -n "$FISH_CHECK_LINT" ] && [ "$FISH_CHECK_LINT" != false ]; then
        echo "Linting is not supported when sanitizing." >&2
        exit 1
    fi
    export FISH_CHECK_LINT=false
    export FISH_TEST_MAX_CONCURRENCY="${FISH_TEST_MAX_CONCURRENCY:-4}"
fi

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
    if [ -n "$FISH_CHECK_RUST_TOOLCHAIN" ]; then
        # shellcheck disable=2086
        command cargo "+$FISH_CHECK_RUST_TOOLCHAIN" "$subcmd" $cargo_args "$@"
    else
        # shellcheck disable=2086
        command cargo "$subcmd" $cargo_args "$@"
    fi
}

cleanup () {
    if [ -n "$gettext_template_dir" ] && [ -e "$gettext_template_dir" ]; then
        rm -r "$gettext_template_dir"
    fi
}

trap cleanup EXIT INT TERM HUP

if $lint; then
    export RUSTFLAGS="--deny=warnings ${RUSTFLAGS}"
    export RUSTDOCFLAGS="--deny=warnings ${RUSTDOCFLAGS}"
fi

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

gettext_template_dir=$(mktemp -d)
(
    export FISH_GETTEXT_EXTRACTION_DIR="$gettext_template_dir"
    cargo build --workspace --all-targets --features=gettext-extract
)
if $lint; then
    if command -v cargo-deny >/dev/null; then
        cargo deny --all-features --locked --exclude-dev check licenses
    fi
    PATH="$build_dir:$PATH" "$workspace_root/build_tools/style.fish" --all --check
    for features in "" --no-default-features; do
        cargo clippy --workspace --all-targets $features
    done
fi
cargo test --no-default-features --workspace --all-targets
cargo test --doc --workspace
if $lint; then
    cargo doc --workspace --no-deps
fi
FISH_GETTEXT_EXTRACTION_DIR=$gettext_template_dir "$workspace_root/tests/test_driver.py" "$build_dir"

exit
}
