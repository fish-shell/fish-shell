#!/bin/sh

set -eux

source=$1
tarball_dir=$2
output=$3

committer=$FISH_COMMITTER # TODO Make sure that this person is listed in debian/control ?
gpg_key_id=$FISH_GPG_KEY_ID
version=$FISH_VERSION

export DEBEMAIL=$committer
export EDITOR=true

mkdir "$output"
# Should be a hard link.
cp "$tarball_dir/fish-$version.tar.xz" "$output/fish_$version.orig.tar.xz"
(
    cd "$output"
    tar xf "fish_$version.orig.tar.xz"
)
tarball_sha256sum=$(sha256sum "$output/fish-$version.tar.xz" | cut -d' ' -f1)

# Copy in the Debian packaging information
cp -r "$source/debian" "$output"/fish-$version/debian

# TODO Include old changes?
echo > "$output"/fish-$version/debian/changelog "\
fish ($version-1) testing; urgency=medium

  * Upstream release.

 -- $committer  $(date)"

# Update the Debian changelog for the new version - interactive
(
    cd "$output"/fish-$version
    dch --newversion "$version-1" --distribution testing
)

(
    cd "$output"
    (
        cd fish-$version
        # Build the package for Debian on OBS.
        debuild -S -k"$gpg_key_id"
        # Set up an array for the Ubuntu packages
        ppa_series=$(
            echo '
                from launchpadlib.launchpad import Launchpad
                launchpad = Launchpad.login_anonymously("fish shell build script", "production", "~/.cache", version="devel")
                ubu = launchpad.projects("ubuntu")
                # dash requires a double backslash for unknown reasons.
                print("\\n".join(x["name"] for x in ubu.series.entries if x["supported"] == True and x["name"] not in ("trusty", "precise")))
            ' |
            sed s,^[[:space:]]*,, |
            python3
        )
        # Build the Ubuntu source packages
        for series in $ppa_series
        do
            sed -i -e "s/^fish ($version-1)/fish ($version-1~$series)/" -e "s/testing/$series/" debian/changelog
            debuild -S -sa -k"$gpg_key_id"
            sed -i -e "s/^fish ($version-1~$series)/fish ($version-1)/" -e "s/$series/testing/" debian/changelog
        done
    )

    # TODO populate the dput config file.
    # # Or other appropriate PPA defined in ~/.dput.cf
    # for i in fish_$version-1~*.changes
    # do
    #     dput fish-release-3 "$i"
    # done
)
