#!/bin/sh

if [ "$#" -gt 2 -o "$#" -eq 0 ]; then
    echo "Usage: driver.sh /path/to/fish [/path/to/other/fish]"
    exit 1
fi

FISH_PATH=$1
FISH2_PATH=$2
BENCHMARKS_DIR=$(dirname "$0")/benchmarks

for benchmark in "$BENCHMARKS_DIR"/*; do
    basename "$benchmark"
    [ -n "$FISH2_PATH" ] && echo "$FISH_PATH"
    ${FISH_PATH} --print-rusage-self "$benchmark" > /dev/null
    if [ -n "$FISH2_PATH" ]; then
        echo "$FISH2_PATH"
        ${FISH2_PATH} --print-rusage-self "$benchmark" > /dev/null
    fi

    if command -v hyperfine >/dev/null 2>&1; then
        if [ -n "$FISH2_PATH" ]; then
            hyperfine "${FISH_PATH} $benchmark > /dev/null" "${FISH2_PATH} $benchmark > /dev/null"
        else
            hyperfine "${FISH_PATH} $benchmark > /dev/null"
        fi
    fi
done

