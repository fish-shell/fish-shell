#!/bin/sh

set -e

repo_root="$(dirname "$0")/../.."
cd "$repo_root"
included_file="$PWD/build_tools/shellcheck/included.txt"

# shellcheck disable=2046
shellcheck $(cat "$included_file")
