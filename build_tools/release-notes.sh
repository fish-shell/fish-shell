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
add_stats=false
# Skip on shallow clone (CI) for now.
if test -z "$CI" || [ "$(git -C "$workspace_root" tag | wc -l)" -gt 1 ]; then {
    previous_version=$(
        cd "$workspace_root"
        git for-each-ref --format='%(objecttype) %(refname:strip=2)' refs/tags |
            awk '/tag/ {print $2}' | sort --version-sort |
            grep -vxF "$(git describe)" | tail -1
    )
    minor_version=${version%.*}
    previous_minor_version=${previous_version%.*}
    if [ "$minor_version" != "$previous_minor_version" ]; then
        add_stats=true
    fi
} fi
{
    sed -n 1,2p <"$workspace_root/CHANGELOG.rst"
    if $add_stats; then {
        ExtractCommitters() {
            git log "$1" --format="%aN"
            trailers='Co-authored-by|Signed-off-by'
            git log "$1" --format="%b" | sed -En "/^($trailers):\s*/{s///;s/\s*<.*//;p}"
        }
        ListCommitters() {
            comm "$@" "$relnotes_tmp/committers-then" "$relnotes_tmp/committers-now"
        }
        (
            cd "$workspace_root"
            ExtractCommitters "$previous_version" | sort -u >"$relnotes_tmp/committers-then"
            ExtractCommitters "$previous_version".. | sort -u >"$relnotes_tmp/committers-now"
            ListCommitters -13 >"$relnotes_tmp/committers-new"
            ListCommitters -12 >"$relnotes_tmp/committers-returning"
            num_commits=$(git log --no-merges --format=%H "$previous_version".. | wc -l)
            num_authors=$(wc -l <"$relnotes_tmp/committers-now")
            num_new_authors=$(wc -l <"$relnotes_tmp/committers-new")
            printf %s \
                "This release comprises $num_commits commits since $previous_version," \
                " contributed by $num_authors authors, $num_new_authors of which are new committers."
            echo
            echo
        )
    } fi

    printf '%s\n' "$(awk <"$workspace_root/CHANGELOG.rst" '
        NR <= 2 || /^\.\. ignore / { next }
        /^===/ { exit }
        { print }
    ' | sed '$d')" |
        sed -e '$s/^----*$//' # Remove spurious transitions at the end of the document.

    if $add_stats; then {
        JoinEscaped() {
            LC_CTYPE=C.UTF-8 sed 's/\S/\\&/g' |
                awk '
                    NR != 1 { printf ",\n" }
                    { printf "%s", $0 }
                    END { printf "\n" }
                '
        }
        echo ""
        echo "---"
        echo ""
        echo "Thanks to everyone who contributed through issue discussions, code reviews, or code changes."
        echo
        printf "Welcome our new committers: "
        JoinEscaped <"$relnotes_tmp/committers-new"
        echo
        printf "Welcome back our returning committers: "
        JoinEscaped <"$relnotes_tmp/committers-returning"
    } fi
    echo
    echo "---"
    echo
    echo 'Download links:'
    echo 'To download the source code for fish, we suggest the file named ``fish-'"$version"'.tar.xz``.'
    echo 'The file downloaded from ``Source code (tar.gz)`` will not build correctly.'
    echo 'A GPG signature using `this key <'"${FISH_GPG_PUBLIC_KEY_URL:-???}"'>`__ is available as ``fish-'"$version"'.tar.xz.asc``.'
    echo
    echo 'The files called ``fish-'"$version"'-linux-*.tar.xz`` contain'
    echo '`standalone fish binaries <https://github.com/fish-shell/fish-shell/?tab=readme-ov-file#building-fish-with-cargo>`__'
    echo 'for any Linux with the given CPU architecture.'
} >"$relnotes_tmp/fake-workspace"/CHANGELOG.rst

sphinx-build >&2 -j auto \
    -W -E -b markdown -c "$workspace_root/doc_src" \
    -d "$relnotes_tmp/doctree" "$relnotes_tmp/fake-workspace/doc_src" "$relnotes_tmp/out" \
    -D markdown_http_base="https://fishshell.com/docs/$minor_version" \
    -D markdown_uri_doc_suffix=".html" \
    -D markdown_flavor=github \
    "$@"

# Skip changelog header
sed -n 1p "$relnotes_tmp/out/relnotes.md" | grep -Fxq "# Release notes"
sed -n 2p "$relnotes_tmp/out/relnotes.md" | grep -Fxq ''
sed 1,2d "$relnotes_tmp/out/relnotes.md"

rm -r "$relnotes_tmp"
