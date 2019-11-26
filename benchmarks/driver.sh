#!/usr/bin/env bash

if [ "$#" -ne 1 ]; then
    echo "Usage: driver.sh /path/to/fish"
fi

FISH_PATH=$1
BENCHMARKS_DIR=$(dirname "$0")/benchmarks

for benchmark in "$BENCHMARKS_DIR"/*; do
    echo $(basename "$benchmark")
    ${FISH_PATH} --print-rusage-self $benchmark > /dev/null
    if hash hyperfine 2>/dev/null; then
        hyperfine "${FISH_PATH} $benchmark > /dev/null"
    fi
done

