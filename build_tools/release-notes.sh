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
previous_version=$(
    cd "$workspace_root"
    awk <CHANGELOG.rst '
        ( /^fish \S*\.\S*\.\S* \(released .*\)$/ &&
            NR > 1 &&
            # Skip tags that have not been created yet..
            system("git rev-parse --verify >/dev/null --quiet refs/tags/"$2) == 0 \
        ) {
            print $2; exit
        }
    '
)
minor_version=${version%.*}
previous_minor_version=${minor_version%.*}
{
    sed -n 1,2p <"$workspace_root/CHANGELOG.rst"

    ListCommitters() {
        comm "$@" "$relnotes_tmp/committers-then" "$relnotes_tmp/committers-now"
    }
    (
        cd "$workspace_root"
        git log "$previous_version" --format="%aN" | sort -u >"$relnotes_tmp/committers-then"
        git log "$previous_version".. --format="%aN" | sort -u >"$relnotes_tmp/committers-now"
        ListCommitters -13 >"$relnotes_tmp/committers-new"
        ListCommitters -12 >"$relnotes_tmp/committers-returning"
    )
    if [ "$minor_version" != "$previous_minor_version" ]; then
        (
            cd "$workspace_root"
            num_commits=$(git log --no-merges --format=%H "$previous_version".. | wc -l)
            num_authors=$(wc -l <"$relnotes_tmp/committers-now")
            num_new_authors=$(wc -l <"$relnotes_tmp/committers-new")
            printf %s \
                "This release comprises $num_commits commits since $previous_version," \
                " contributed by $num_authors authors, $num_new_authors of which are new committers."
            echo
            echo
        )
    fi

    printf %s "$(awk <"$workspace_root/CHANGELOG.rst" '
        NR <= 2 || /^\.\. ignore / { next }
        /^===/ { exit }
        { print }
    ' | sed '$d')" |
        sed -e '$s/^----*$//' # Remove spurious transitions at the end of the document.

    JoinEscaped() {
        sed 's/\S/\\&/g' |
            awk '
                NR != 1 { printf ", " }
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
    echo
    echo "---"
    echo
    echo "*Download links: To download the source code for fish, we suggest the file named \"fish-$version.tar.xz\". The file downloaded from \"Source code (tar.gz)\" will not build correctly.*"
    echo
    echo "*The files called fish-$version-linux-\*.tar.xz are experimental packages containing a single standalone ``fish`` binary for any Linux with the given CPU architecture.*"
} >"$relnotes_tmp/fake-workspace"/CHANGELOG.rst

sphinx-build >&2 -j auto \
    -W -E -b markdown -c "$workspace_root/doc_src" \
    -d "$relnotes_tmp/doctree" "$relnotes_tmp/fake-workspace/doc_src" "$relnotes_tmp/out" \
    -D markdown_http_base="https://fishshell.com/docs/$minor_version" \
    -D markdown_uri_doc_suffix=".html" \
    -D markdown_github_flavored=1 \
    "$@"

# Skip changelog header
sed -n 1p "$relnotes_tmp/out/relnotes.md" | grep -Fxq "# Release notes"
sed -n 2p "$relnotes_tmp/out/relnotes.md" | grep -Fxq ''
sed 1,2d "$relnotes_tmp/out/relnotes.md"

rm -r "$relnotes_tmp"
