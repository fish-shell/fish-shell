#!/bin/sh

{

set -ex

version=$1
repository_owner=fish-shell
remote=origin
if [ -n "$2" ]; then
    set -u
    repository_owner=$2
    remote=$3
    set +u
    [ $# -eq 3 ]
fi

[ -n "$version" ]

for tool in \
    bundle \
    gh \
    jq \
    ruby \
    timeout \
; do
    if ! command -v "$tool" >/dev/null; then
        echo >&2 "$0: missing command: $1"
        exit 1
    fi
done

repo_root="$(dirname "$0")/.."
fish_site=$repo_root/../fish-site
fish_site_repo=git@github.com:$repository_owner/fish-site

for path in . "$fish_site"
do
    if ! git -C "$path" diff HEAD --quiet ||
        git ls-files --others --exclude-standard | grep .; then
        echo >&2 "$0: index and worktree must be clean"
        exit 1
    fi
done

(
    cd "$fish_site"
    [ "$(git rev-parse HEAD)" = \
        "$(git ls-remote "$fish_site_repo" refs/heads/master |
            awk '{print $1}')" ]
)

if git tag | grep -qxF "$version"; then
    echo >&2 "$0: tag $version already exists"
    exit 1
fi

integration_branch=$(
    git for-each-ref --points-at=HEAD 'refs/heads/Integration_*' \
        --format='%(refname:strip=2)'
)
[ -n "$integration_branch" ] ||
    git merge-base --is-ancestor $remote/master HEAD

sed -n 1p CHANGELOG.rst | grep -q '^fish .*(released .*)$'
sed -n 2p CHANGELOG.rst | grep -q '^===*$'

changelog_title="fish $version (released $(date +'%B %d, %Y'))"
sed -i \
    -e "1c$changelog_title" \
    -e "2c$(printf %s "$changelog_title" | sed s/./=/g)" \
    CHANGELOG.rst

CommitVersion() {
    sed -i "s/^version = \".*\"/version = \"$1\"/g" Cargo.toml
    cargo fetch --offline
    git add CHANGELOG.rst Cargo.toml Cargo.lock
    git commit -m "$2

Created by ./build_tools/release.sh $version"
}

CommitVersion "$version" "Release $version"

# N.B. this is not GPG-signed.
git tag --annotate --message="Release $version" $version

git push $remote $version

TIMEOUT=
gh() {
    command ${TIMEOUT:+timeout $TIMEOUT} \
        gh --repo "$repository_owner/fish-shell" "$@"
}

gh workflow run release.yml --ref="$version" \
    --raw-field="version=$version"

run_id=
while [ -z "$run_id" ] && sleep 5
do
    run_id=$(gh run list \
                --json=databaseId --jq=.[].databaseId \
                --workflow=release.yml --limit=1 \
                --commit="$(git rev-parse "$version^{commit}")")
done

# Update fishshell.com
tag_oid=$(git rev-parse "$version")
tmpdir=$(mktemp -d)
# TODO This works on draft releases only if "gh" is configured to
# have write access to the fish-shell repository. Unless we are fine
# publishing the release at this point, we should at least fail if
# "gh" doesn't have write access.
while ! \
    gh release download "$version" --dir="$tmpdir" \
        --pattern="fish-$version.tar.xz"
do
    TIMEOUT=30 gh run watch "$run_id" ||:
    sleep 5
done
actual_tag_oid=$(git ls-remote "$remote" |
    awk '$2 == "refs/tags/'"$version"'" { print $1 }')
[ "$tag_oid" = "$actual_tag_oid" ]
( cd "$tmpdir" && tar xf fish-$version.tar.xz )
CopyDocs() {
    rm -rf "$fish_site/site/docs/$1"
    cp -r "$tmpdir/fish-$version/user_doc/html" "$fish_site/site/docs/$1"
    git -C $fish_site add "site/docs/$1"
}
minor_version=${version%.*}
CopyDocs "$minor_version"
latest_release=$(
    releases=$(git tag | grep '^[0-9]*\.[0-9]*\.[0-9]*.*' |
        sed $(: "De-prioritize release candidates (1.2.3-rc0)") \
        's/-/~/g' | LC_ALL=C sort --version-sort)
    printf %s\\n "$releases" | tail -1
)
if [ "$version" = "$latest_release" ]; then
    CopyDocs current
fi
rm -rf "$tmpdir"
(
    cd "$fish_site"
    make
    git add -u
    git add docs
    if git ls-files --others --exclude-standard | grep .; then
        exit 1
    fi
    git commit --message="$(printf %s "\
        | Release $version (docs)
        |
        | Created by ../fish-shell/build_tools/release.sh
    " | sed 's,^\s*| \?,,')"
)

gh_api_repo() {
    path=$1
    shift
    command gh api \
        -H "Accept: application/vnd.github+json" \
        -H "X-GitHub-Api-Version: 2022-11-28" \
        "/repos/$repository_owner/fish-shell/$1" \
        "$@"
}

# Approve macos-codesign
# TODO what if current user can't approve?
gh_pending_deployments() {
    gh_api_repo "actions/runs/$run_id/pending_deployments" "$@"
}
while {
    environment_id=$(gh_pending_deployments | jq .[].environment.id)
    [ -z "$environment_id" ]
}
do
    sleep 5
done
echo '
        {
            "environment_ids": ['"$environment_id"'],
            "state": "approved",
            "comment": "Approved via ./build_tools/release.sh"
        }
    ' |
gh_pending_deployments --method POST --input=-

# Await completion.
gh run watch "$run_id"

while {
    ! draft=$(gh release view "$version" --json=isDraft --jq=.isDraft) \
    || [ "$draft" = true ]
}
do
    sleep 20
done

(
    cd "$fish_site"
    make new-release
    git add -u
    git add docs
    if git ls-files --others --exclude-standard | grep .; then
        exit 1
    fi
    git commit --message="$(printf %s "\
        | Release $version (release list update)
        |
        | Created by ../fish-shell/build_tools/release.sh
    " | sed 's,^\s*| \?,,')"
    # This takes care to support remote names that are different from
    # fish-shell remote name. Also, support detached HEAD state.
    git push "$fish_site_repo" HEAD:master
)

if [ -n "$integration_branch" ]; then {
    git push $remote "$version^{commit}":refs/heads/$integration_branch
} else {
    changelog=$(cat - CHANGELOG.rst <<EOF
fish ?.?.? (released ???)
=========================

EOF
    )
    printf %s\\n "$changelog" >CHANGELOG.rst
    CommitVersion ${version}-snapshot "start new cycle"
    git push $remote HEAD:master
} fi

milestone_number=$(
    gh_api_repo milestones?state=open |
        jq '.[] | select(.title == "fish '"$version"'") | .number'
)
gh_api_repo --method PATCH milestones/$milestone_number \
    --raw-field state=closed

next_patch_version=$(
    echo "$version" | awk -F. '
        NF == 3 && $3 ~ /[0-9]+/ {
            printf "%s.%s.%s", $1, $2, $3+1
        }
    '
)
if [ -n "$next_patch_version" ]; then
    gh_api_repo --method POST milestones \
        --raw-field title="fish $next_patch_version"
fi

exit

}
