#!/bin/sh

{
set -ex

lint=true
if [ "$FISH_CHECK_LINT" = false ]; then
    lint=false
fi

case "$(uname)" in
    MSYS*)
        is_cygwin=true
        cygwin_var=MSYS
        ;;
    CYGWIN*)
        is_cygwin=true
        cygwin_var=CYGWIN
        ;;
    *)
        is_cygwin=false
        ;;
esac

check_dependency_versions=false
if [ "${FISH_CHECK_DEPENDENCY_VERSIONS:-false}" != false ]; then
    check_dependency_versions=true
fi

green='\e[0;32m'
yellow='\e[1;33m'
reset='\e[m'

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

# shellcheck disable=2317,2329
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

gettext_template_dir=$(mktemp -d)
(
    # shellcheck disable=2030
    export FISH_GETTEXT_EXTRACTION_DIR="$gettext_template_dir"
    cargo build --workspace --all-targets --features=gettext-extract
)
if $lint; then
    if command -v cargo-deny >/dev/null; then
        cargo deny --all-features --locked --exclude-dev check licenses
    fi

    if command -v shellcheck >/dev/null || { test -n "$CI" && ! $is_cygwin; }; then
        cargo xtask shellcheck
    fi

    PATH="$build_dir:$PATH" cargo xtask format --all --check
    for features in "" --no-default-features --all-features; do
        cargo clippy --workspace --all-targets $features
    done

    cargo xtask gettext --rust-extraction-dir="$gettext_template_dir" check
fi

# When running `cargo test`, some binaries (e.g. `fish_gettext_extraction`)
# are dynamically linked against Rust's `std-xxx.dll` instead of being
# statically link as they usually are.
# On Cygwin, `PATH`is not properly updated to point to the `std-xxx.dll`
# location, so we have to do it manually.
# See:
# - https://github.com/rust-lang/rust/issues/149050
# - https://github.com/msys2/MSYS2-packages/issues/5784
(
    if $is_cygwin; then
        PATH="$PATH:$(rustc --print target-libdir)"
        export PATH
    fi
    cargo test --no-default-features --workspace --all-targets
)
cargo test --doc --workspace

if $lint; then
    cargo doc --workspace --no-deps
fi

system_tests() {
    "$workspace_root/tests/test_driver.py" "$build_dir" "$@"
}

if $is_cygwin; then
    # shellcheck disable=2059
    printf "=== Running ${green}integration tests ${yellow}with${green} symlinks${reset}\n"
    (
        export "$cygwin_var"=winsymlinks
        system_tests
    )

    # shellcheck disable=2059
    printf "=== Running ${green}integration tests ${yellow}without${green} symlinks${reset}\n"
    (
        # Only redo the tests that use `ln` to saves some time
        export "$cygwin_var"=
        # shellcheck disable=2046
        system_tests $(grep -l -E '\bln\b' -r tests/checks/)
    )
else
    # shellcheck disable=2059
    printf "=== Running ${green}integration tests${reset}\n"
    system_tests
fi

exit
}
