#!/bin/sh

set -e

workspace_root=$(dirname "$0")/..

relnotes_tmp=$(mktemp -d)
mkdir -p "$relnotes_tmp/fake-workspace" "$relnotes_tmp/out"
(
    cd "$workspace_root"
    cp -r doc_src CONTRIBUTING.rst README.rst "$relnotes_tmp/fake-workspace"
)
version=$(sed 's,^fish \(\S*\) .*,\1,; 1q' "$workspace_root/CHANGELOG.rst")
previous_version=$(awk <"$workspace_root/CHANGELOG.rst" '
    ( /^fish \S*\.\S*\.\S* \(released .*\)$/ &&
        NR > 1 &&
        # Skip tags that have not been created yet..
        system("git rev-parse --verify >/dev/null --quiet refs/tags/"$2) == 0 \
    ) {
        print $2; exit
    }
')
minor_version=${version%.*}
changelog_for_this_version=$(awk <"$workspace_root/CHANGELOG.rst" '
    /^===/ { if (v++) { exit } }
    { print }
' | sed '$d'
)
# Remove spurious transitions at the end of the document.
printf %s "$changelog_for_this_version" |
    sed -e '$s/^----*$//' >"$relnotes_tmp/fake-workspace"/CHANGELOG.rst

# Use "-j 1" because sphinx-markdown-builder is not marked concurrency-safe.
sphinx-build >&2 -j 1 \
    -W -E -b markdown -c "$workspace_root/doc_src" \
    -d "$relnotes_tmp/doctree" "$relnotes_tmp/fake-workspace/doc_src" $relnotes_tmp/out \
    -D markdown_http_base="https://fishshell.com/docs/$minor_version" \
    -D markdown_uri_doc_suffix=".html" \
    "$@"

# Delete changelog header
sed -n 1p "$relnotes_tmp/out/relnotes.md" | grep -Fxq "# Release notes"
sed -n 2p "$relnotes_tmp/out/relnotes.md" | grep -Fxq ''
sed -i 1,2d "$relnotes_tmp/out/relnotes.md"

{
    cat "$relnotes_tmp/out/relnotes.md"
    echo ""
    echo "---"
    echo ""
    "$workspace_root"/build_tools/list_committers_since.fish "$previous_version"
    cat <<EOF

---

*Download links: To download the source code for fish, we suggest the file named "fish-$version.tar.xz". The file downloaded from "Source code (tar.gz)" will not build correctly.*

*The files called fish-$version-linux-\*.tar.xz are experimental packages containing a single standalone ``fish`` binary for any Linux with the given architecture.*
EOF
}
rm -r "$relnotes_tmp"
