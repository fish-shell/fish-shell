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
    sort --version-sort </dev/null
    # Check that we have latest stable.
    if rustup check | grep ^stable- | grep 'Update available'; then
        exit 1
    fi
    # Check that GitHub CI uses latest stable Rust version.
    stable_rust_version=$($(rustup +stable which rustc) --version | cut -d' ' -f2)
    stable_rust_version=${stable_rust_version%.*}
    grep -q "(stable) echo $stable_rust_version ;;" \
        "$(project-root)/.github/actions/rust-toolchain/action.yml"

    # Check that GitHub CI's MSRV is the Rust version available in Debian stable.
    debian_stable_codename=$(
        curl -fsS https://ftp.debian.org/debian/dists/stable/Release |
        grep '^Codename:' | cut -d' ' -f2)
    debian_stable_rust_version=$(
    	curl -fsS https://sources.debian.org/api/src/rustc/ |
            jq -r --arg debian_stable_codename "$debian_stable_codename" '
            	.versions[] | select(.suites[] == $debian_stable_codename) | .version' |
            sed 's/^\([0-9]\+\.[0-9]\+\).*/\1/' |
            sort --version-sort |
            tail -1)
    grep -q "(msrv) *echo $debian_stable_rust_version ;;" \
        "$(project-root)/.github/actions/rust-toolchain/action.yml"

    # Check that Cirrus CI uses the latest minor version of Alpine Linux.
    alpine_latest_version=$(
        curl -fsS "https://registry.hub.docker.com/v2/repositories/library/alpine/tags?page_size=10" |
          jq -r '.results[].name' | grep '^[0-9]\+\.[0-9]\+$' |
          sort --version-sort |
          tail -1)
    grep -qFx "FROM alpine:$alpine_latest_version" docker/alpine.Dockerfile

    # Check that Cirrus CI uses the oldest version of Ubuntu that's not EOL.
    ubuntu_oldest_alive_version=$(
        today=$(date --iso-8601)
        curl -fsS https://endoflife.date/api/ubuntu.json |
            jq -r --arg today "$today" '
                .[]
                | select(.eol >= $today)
                | "\(.cycle)"
            ' |
            sort --version-sort |
            head -1
    )
    grep -qFx "FROM ubuntu:$ubuntu_oldest_alive_version" \
        docker/ubuntu-oldest-supported.Dockerfile
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

if [ -n "$FISH_TEST_MAX_CONCURRENCY" ]; then
    export RUST_TEST_THREADS="$FISH_TEST_MAX_CONCURRENCY"
    export CARGO_BUILD_JOBS="$FISH_TEST_MAX_CONCURRENCY"
fi

template_file=$(mktemp)
(
    export FISH_GETTEXT_EXTRACTION_FILE="$template_file"
    cargo build --workspace --all-targets --features=gettext-extract
)
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
