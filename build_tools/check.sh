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
    # shellcheck disable=2317
    if [ -n "$template_file" ] && [ -e "$template_file" ]; then
        rm "$template_file"
    fi
}

trap cleanup EXIT INT TERM HUP

if $lint; then
    export RUSTFLAGS="--deny=warnings ${RUSTFLAGS}"
    export RUSTDOCFLAGS="--deny=warnings ${RUSTDOCFLAGS}"
fi

repo_root="$(dirname "$0")/.."
build_dir="${CARGO_TARGET_DIR:-$repo_root/target}/${target_triple}/debug"

template_file=$(mktemp)
FISH_GETTEXT_EXTRACTION_FILE=$template_file cargo build --workspace --all-targets --features=gettext-extract
if $lint; then
    PATH="$build_dir:$PATH" "$repo_root/build_tools/style.fish" --all --check

    shellcheck_dir="$repo_root/build_tools/shellcheck"
    if command -v rg; then
        echo 'Checking if list of files to shellcheck is up to date.'
        echo 'If this fails, you can update the list automatically using'
        echo "$shellcheck_dir/check_for_file_updates.sh --update"
        "$shellcheck_dir/check_for_file_updates.sh"
    else
        echo 'Cannot update list of files to shellcheck. Requires ripgrep.'
    fi
    if command -v shellcheck; then
        "$shellcheck_dir/check.sh"
    else
        echo 'Error: ShellCheck is not available.'
        exit 1
    fi

    cargo clippy --workspace --all-targets
fi
cargo test --no-default-features --workspace --all-targets
cargo test --doc --workspace
if $lint; then
    cargo doc --workspace
fi
FISH_GETTEXT_EXTRACTION_FILE=$template_file "$repo_root/tests/test_driver.py" "$build_dir"

exit
}
