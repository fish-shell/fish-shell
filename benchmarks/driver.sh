#!/bin/sh

if [ "$#" -ne 1 ]; then
    echo "Usage: driver.sh /path/to/fish"
fi

FISH_PATH=$1
BENCHMARKS_DIR=$(dirname "$0")/benchmarks

for benchmark in "$BENCHMARKS_DIR"/*; do
    basename "$benchmark"
    ${FISH_PATH} --print-rusage-self "$benchmark" > /dev/null
    if command -v hyperfine >/dev/null 2>&1; then
        hyperfine "${FISH_PATH} $benchmark > /dev/null"
    fi
done

