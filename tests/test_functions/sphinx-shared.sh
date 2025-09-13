#!/bin/sh

set -e

cleanup () {
    if [ -n "$tmp_dir" ] && [ -e "$tmp_dir" ]; then
        rm -r "$tmp_dir"
    fi
}

trap cleanup EXIT INT TERM HUP

workspace_root=$(dirname "$0")/../..
builder=$1
docsrc=$workspace_root/doc_src
tmp_dir=$(mktemp -d)
doctree=$tmp_dir/doctree
output_dir=$tmp_dir/$builder
sphinx-build \
    -j auto \
    -q \
    -W \
    -E \
    -b "$builder" \
    -c "$docsrc" \
    -d "$doctree" \
    "$docsrc" \
    "$output_dir"
