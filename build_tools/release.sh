#!/bin/sh

{

set -ex

usage() {
    cat << EOF
Usage: [FISH_GPG_KEY_ID=<KEY>] $(basename $0) [OPTIONS] <version>

Build and publish a release with the given version.

Release artifacts will be left behind in "./release" for debugging.

On failure, an attempted is made to restore the state.
Same if --dry-run is given, even if there was no failure.

$FISH_GPG_KEY_ID defaults to your Git author identity.

Options:
    -n, --dry-run     Do not publish the release
    --no-test         Skip running tests.
    --no-rebuild      Do not rebuild tarball
    --no-rebuild-debs Do not rebuild Debian packages
EOF
    exit 1
}

build_debian_packages() {
    dockerfile=$PWD/build_tools/release/build-debian-package.Dockerfile

    export DOCKER_BUILDKIT=1

    # Construct a docker image.
    img_tagname="ghcr.io/fish-shell/fish-ci/$(basename -s .Dockerfile "$dockerfile"):latest"
    sudo=sudo
    if groups | grep -q '\bdocker\b'; then
        sudo=
    fi
    $sudo docker build \
        -t "$img_tagname" \
        -f "$dockerfile" \
        "$PWD"/build_tools/release/

    tmpdir=$(mktemp -d)
    $sudo docker run -it \
        --cidfile="$tmpdir/container_id" \
        --mount type=bind,source="$PWD",target=/fish-source,readonly \
        --mount type=bind,source="${GNUPGHOME:-"$HOME/.gnupg"}",target=/gnupg,readonly \
        --env "FISH_COMMITTER=$committer" \
        --env "FISH_GPG_KEY_ID=$gpg_key_id" \
        --env "FISH_VERSION=$version" \
        "$img_tagname"

    container_id=$(cat "$tmpdir/container_id")

    $sudo docker cp "$container_id":/home/fishuser/dpkgs release/dpkgs
    $sudo chown -R $USER release/dpkgs
    $sudo docker rm "$container_id"
    $sudo rm -r "$tmpdir"
}

committer="$(git var GIT_AUTHOR_IDENT)"
committer=${committer% *} # strip timezone
committer=${committer% *} # strip timestamp
gpg_key_id=${FISH_GPG_KEY_ID:-"$committer"}

# TODO Check for user errors here (in case HEAD is not pushed yet).
is_latest_release=false
if git merge-base --is-ancestor origin/master HEAD; then
    is_latest_release=true
fi

version=
dry_run=
do_test=true
do_rebuild=true
do_rebuild_debs=true
for arg
do
    case "$arg" in
        (-n|--dry-run) dry_run=: ;;
        (--no-test) do_test=false ;;
        (--no-rebuild) do_rebuild=false ;;
        (--no-rebuild-debs) do_rebuild_debs=false ;;
        (-*) usage ;;
        (*)
            if [ -n "$version" ]; then
                usage
            fi
            version=$arg
            ;;
    esac
done

if [ -z "$version" ]; then
    usage
fi

for tool in \
    gh \
    gpg \
    osc \
    pip \
    sphinx-build \
    virtualenv \
; do
    if ! command -v "$tool" >/dev/null; then
        echo >&2 "$0: missing command: $1"
        exit 1
    fi
done

if ! echo "$version" | grep '^3\.'; then
    echo >&2 "$0: major version bump needs changes to PPA code"
    exit 1
fi

if [ -e release ] && $do_rebuild ; then
    echo >&2 "$0: ./release exists, please remove it or pass --no-rebuild to reuse the tarball"
    exit 1
fi

for repo in . ../fish-site
do
    if ! git -C "$repo" diff HEAD --quiet; then
        echo >&2 "$0: index and worktree must be clean"
        exit 1
    fi
done

if git tag | grep -qxF "$version"; then
    echo >&2 "$0: tag $version already exists"
    exit 1
fi

cleanup_done=false
did_commit=false
cleanup_after_error_or_dry_run() {
    printf '\n\n'
    if $cleanup_done; then
        return
    fi
    cleanup_done=true
    {
        set +e
        if $did_commit; then
            # Try to only undo our changelog changes.
            git revert --no-edit HEAD &&
                git reset HEAD~2
        fi
        git tag -d $version
        exit 1
    } >/dev/null
}
trap cleanup_after_error_or_dry_run INT EXIT

if $do_test; then
    ninja -Cbuild test
fi

sed -i "1cfish $version (released $(date +'%B %d, %Y'))" CHANGELOG.rst
git add CHANGELOG.rst
git commit -m "Release $version"
did_commit=true # TODO This is hacky.
git -c "user.signingKey=$gpg_key_id" tag -a --message="Release $version" $version -s

if ! [ -d release ] || $do_rebuild; then
    mkdir -p release
    FISH_ARTEFACT_PATH=$PWD/release build_tools/make_tarball.sh
    gpg --local-user "$gpg_key_id" --sign --detach --armor "release/fish-$version.tar.xz"
fi

if ! [ -d release/dpkgs ] || $do_rebuild_debs; then
    build_debian_packages
fi

rm -rf release/obs
mkdir -p release/obs
(
    cd release/obs
    osc checkout shells:fish:release:3/fish
    cd shells:fish:release:3/fish
    rm -f *.tar.* *.dsc
)
ln release/dpkgs/fish_$version.orig.tar.xz release/obs/shells:fish:release:3/fish
ln release/dpkgs/fish_$version-1.debian.tar.xz release/obs/shells:fish:release:3/fish
ln release/dpkgs/fish_$version-1.dsc release/obs/shells:fish:release:3/fish
rpmversion=$(echo $version | sed -e 's/-/+/' -e 's/-/./g')
sed -e "s/@version@/$version/g" -e "s/@RPMVERSION@/$rpmversion/g" \
    < "release/dpkgs/fish-$version/fish.spec.in" > release/obs/shells:fish:release:3/fish/fish.spec

(
    cd release/obs/shells:fish:release:3/fish
    osc addremove
)

relnotes_tmp=$(mktemp -d)
(
    virtualenv release/.venv
    . release/.venv/bin/activate
    pip install sphinx-markdown-builder
    # We only need to build latest relnotes. I'm not sure if that's
    # possible, so resort to building everything.
    mkdir $relnotes_tmp/src $relnotes_tmp/out
    sphinx-build -j 8 -b markdown -n "release/dpkgs/fish-$version/doc_src" \
        -d "release/dpkgs/fish-$version/user_doc/doctrees" \
        $relnotes_tmp/markdown
    # Delete title
    sed -i 1,3d "$relnotes_tmp/markdown/relnotes.md"
    # Delete notes for prior releases.
    sed -i '/^## /,$d' "$relnotes_tmp/markdown/relnotes.md"
)

if ! [ -n $dry_run ]; then
    trap '' INT EXIT
fi
$dry_run git push origin "$version" # Push the tag
(
    cd release/obs
    $dry_run osc commit -m "New release: $version"
)
$dry_run gh release create --draft \
    --title=$(head -n 1 "release/dpkgs/fish-$version/CHANGELOG.rst") \
    --notes-file="$relnotes_tmp/markdown/relnotes.md" \
    --verify-tag \
    "$version" \
    "release/fish-$version.tar.xz" "release/fish-$version.tar.xz.asc"
if $is_latest_release; then
    $dry_run git push origin $version:master
fi
rm -rf $relnotes_tmp

(set +x
echo "\
To update fish-site we currently query the github release API
Please publish the draft at https://github.com/fish-shell/fish-shell/releases/tag/$version
and hit Enter to continue with updating ../fish-site")
read line

minor_version=${version%.*}
if $is_latest_release; then
    rm -rf "../fish-site/site/docs/current"
    cp -r "release/dpkgs/fish-$version/user_doc/html" ../fish-site/site/docs/current
fi
rm -rf "../fish-site/site/docs/$minor_version"
cp -r "release/dpkgs/fish-$version/user_doc/html" "../fish-site/site/docs/$minor_version"
(
    cd ../fish-site
    make new-release
    git add -u
    git add site/docs/$minor_version docs/docs/$minor_version
    git add site/docs/current        docs/docs/current
    git commit --message="Release $version"
    $dry_run git push
    if [ -n $dry_run ]; then
        git reset --hard HEAD~ >/dev/null
    fi
)

cat <<EOF
Remaining work:
- Create macOS packages:
    ./build_tools/make_pkg.sh
    ./build_tools/mac_notarize.sh ~/fish_built/fish-$version.pkg <AC_USER>
    ./build_tools/mac_notarize.sh ~/fish_built/fish-$version.app.zip <AC_USER>
- Add macOS packages to https://github.com/fish-shell/fish-shell/releases/tag/$version
- Send email to "fish-users Mailing List <fish-users@lists.sourceforge.net>"
EOF

exit

}
