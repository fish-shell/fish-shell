#!/bin/sh

if [ "$#" -gt 2 ] || [ "$#" -eq 0 ]; then
    echo "Usage: driver.sh /path/to/fish [/path/to/other/fish]"
    exit 1
fi

FISH_PATH=$1
FISH2_PATH=$2
BENCHMARKS_DIR=$(dirname "$0")/benchmarks

quote() {
    # Single-quote the given string for a POSIX shell, except in common cases that don't need it.
    printf %s "$1" |
        sed "/[^[:alnum:]\/.-]/ {
            s/'/'\\\''/g
            s/^/'/
            s/\$/'/
        }"
}

for benchmark in "$BENCHMARKS_DIR"/*; do
    basename "$benchmark"
    # If we have hyperfine, use it first to warm up the cache
    if command -v hyperfine >/dev/null 2>&1; then
        cmd1="$(quote "${FISH_PATH}") --no-config $(quote "$benchmark")"
        if [ -n "$FISH2_PATH" ]; then
            cmd2="$(quote "${FISH2_PATH}") --no-config $(quote "$benchmark")"
            hyperfine --warmup 3 "$cmd1" "$cmd2"
        else
            hyperfine --warmup 3 "$cmd1"
        fi
    fi

    [ -n "$FISH2_PATH" ] && echo "$FISH_PATH"
    "${FISH_PATH}" --print-rusage-self "$benchmark" > /dev/null
    if [ -n "$FISH2_PATH" ]; then
        echo "$FISH2_PATH"
        "${FISH2_PATH}" --print-rusage-self "$benchmark" > /dev/null
    fi

done

