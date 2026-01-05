#!/bin/sh

workspace_root="$(realpath "$(dirname "$0")/..")"

# Variables used at build time

export RUSTFLAGS="-Zsanitizer=address $RUSTFLAGS"
export RUSTDOCFLAGS="-Zsanitizer=address $RUSTDOCFLAGS"

# Variables used at runtime

export ASAN_OPTIONS=check_initialization_order=1:detect_stack_use_after_return=1:detect_leaks=1:fast_unwind_on_malloc=0
export LSAN_OPTIONS=verbosity=0:log_threads=0:print_suppressions=0:suppressions="$workspace_root"/build_tools/lsan_suppressions.txt
export FISH_CI_SAN=1
