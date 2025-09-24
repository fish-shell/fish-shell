#!/bin/sh

set -e

workspace_root=$(dirname "$0")/..

relnotes_tmp=$(mktemp -d)
mkdir "$relnotes_tmp/src" "$relnotes_tmp/out"
version=$(sed 's,^fish \(\S*\) .*,\1,; 1q' "$workspace_root/CHANGELOG.rst")
minor_version=${version%.*}
changelog_for_this_version=$(awk <"$workspace_root/CHANGELOG.rst" '
    /^===/ { if (v++) { exit } }
    { print }
' | sed '$d')
# Also fix up any relative references to other documentation files.
# Also remove spurious transitions at the end of the document.
printf %s "$changelog_for_this_version" |
    sed >"$relnotes_tmp/src"/index.rst \
        -e 's,:doc:`\(.*\) <\([^>]*\)>`,`\1 <https://fishshell.com/docs/'"$minor_version"'/\2.html>`_,g' \
        -e 's,:envvar:`\([^`]*\)`,``$\1``,g' \
        -e '$s/^----*$//'

# Use "-j 1" because sphinx-markdown-builder is not marked concurrency-safe.
sphinx-build >&2 -j 1 \
    -W -E -b markdown -c "$workspace_root/doc_src" \
    -d "$relnotes_tmp/doctree" "$relnotes_tmp/src" $relnotes_tmp/out "$@"
{
    cat "$relnotes_tmp/out/index.md" - <<EOF

--

*Download links: To download the source code for fish, we suggest the file named "fish-$version.tar.xz". The file downloaded from "Source code (tar.gz)" will not build correctly.*

*The files called fish-$version-linux-\*.tar.xz are experimental packages containing a single standalone ``fish`` binary for any Linux with the given architecture.*
EOF
}
rm -r "$relnotes_tmp"
