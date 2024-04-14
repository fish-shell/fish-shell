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

cargo fetch --offline
if ! git diff HEAD --quiet; then
    echo >&2 "$0: index and worktree must be clean (for git describe)"
    exit 1
fi

if git tag | grep -qxF "$version"; then
    echo >&2 "$0: tag $version already exists"
    exit 1
fi

sed -n 1p CHANGELOG.rst | grep -q '^fish .*(released ???)$'
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
    git commit -m "$2"
}

CommitVersion "$version" "Release $version"

# N.B. this is not GPG-signed.
git tag --annotate --message="Release $version" $version

git push $remote $version

if command -v gh >/dev/null; then
    gh_repo() {
        gh --repo "$repository_owner/fish-shell" "$@"
    }
    run_id=
    while [ -z "$run_id" ] && sleep 5
    do
        run_id=$(gh_repo run list \
                    --json=databaseId --jq=.[].databaseId \
                    --workflow=release.yml --limit=1 \
                    --commit="$(git rev-parse "$version^{commit}")")
    done
    gh_repo run watch "$run_id"
    # N.B. --exit-status doesn't fail reliably.
    gh_repo run view "$run_id" --verbose --log-failed --exit-status
fi

./build_tools/wait-for-release.sh \
    "$version" "$(git rev-parse $version)" \
    "$repository_owner" "$remote"

if git merge-base --is-ancestor $remote/master $version
then
    git push $remote $version:master
else
    # Probably on an integration branch.
    :
fi

if [ "$repository_owner" = fish-shell ]; then {
    mail=$(mktemp)
    cat >$mail <<EOF
From: $(git var GIT_AUTHOR_IDENT | sed 's/ [0-9]* +[0-9]*$//')
To: fish-users Mailing List <fish-users@lists.sourceforge.net>
Subject: fish $version released

See https://github.com/fish-shell/fish-shell/releases/tag/$version
EOF
    git send-email --suppress-cc=all $mail
    rm $mail
} fi

changelog=$(cat - CHANGELOG.rst <<EOF
fish ?.?.? (released ???)
=========================

EOF
)
printf %s\\n "$changelog" >CHANGELOG.rst
CommitVersion ${version}-snapshot "start new cycle"

exit

}
