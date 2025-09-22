#!/usr/bin/env bash

# Script to produce an OS X installer .pkg and .app(.zip)

usage() {
    echo "Build macOS packages, optionally signing and notarizing them."
    echo "Usage: $0 options"
    echo "Options:"
    echo "  -s                            Enables code signing"
    echo "  -f <APP_KEY.p12>              Path to .p12 file for application signing"
    echo "  -i <INSTALLER_KEY.p12>        Path to .p12 file for installer signing"
    echo "  -p <PASSWORD>                 Password for the .p12 files (necessary to access the certificates)"
    echo "  -e <entitlements file>        (Optional) Path to an entitlements XML file"
    echo "  -n                            Enables notarization. This will fail if code signing is not also enabled."
    echo "  -j <API_KEY.JSON>             Path to JSON file generated with \`rcodesign encode-app-store-connect-api-key\` (required for notarization)"
    echo
    exit 1
}

set -x
set -e

SIGN=
NOTARIZE=

ARM64_DEPLOY_TARGET='MACOSX_DEPLOYMENT_TARGET=11.0'
X86_64_DEPLOY_TARGET='MACOSX_DEPLOYMENT_TARGET=10.9'

# As of this writing, the most recent Rust release supports macOS back to 10.12.
# The first supported version of macOS on arm64 is 10.15, so any Rust is fine for arm64.
# We wish to support back to 10.9 on x86-64; the last version of Rust to support that is
# version 1.73.0.
RUST_VERSION_X86_64=1.70.0

while getopts "sf:i:p:e:nj:" opt; do
    case $opt in
        s) SIGN=1;;
        f) P12_APP_FILE=$(realpath "$OPTARG");;
        i) P12_INSTALL_FILE=$(realpath "$OPTARG");;
        p) P12_PASSWORD="$OPTARG";;
        e) ENTITLEMENTS_FILE=$(realpath "$OPTARG");;
        n) NOTARIZE=1;;
        j) API_KEY_FILE=$(realpath "$OPTARG");;
        \?) usage;;
    esac
done

if [ -n "$SIGN" ] && { [ -z "$P12_APP_FILE" ] || [ -z "$P12_INSTALL_FILE" ] || [ -z "$P12_PASSWORD" ]; }; then
    usage
fi

if [ -n "$NOTARIZE" ] && [ -z "$API_KEY_FILE" ]; then
    usage
fi

VERSION=$(build_tools/git_version_gen.sh --stdout 2>/dev/null)

echo "Version is $VERSION"

PKGDIR=$(mktemp -d)
echo "$PKGDIR"

SRC_DIR=$PWD
OUTPUT_PATH=${FISH_ARTEFACT_PATH:-~/fish_built}

mkdir -p "$PKGDIR/build_x86_64" "$PKGDIR/build_arm64" "$PKGDIR/root" "$PKGDIR/intermediates" "$PKGDIR/dst"

# Build and install for arm64.
# Pass FISH_USE_SYSTEM_PCRE2=OFF because a system PCRE2 on macOS will not be signed by fish,
# and will probably not be built universal, so the package will fail to validate/run on other systems.
# Note CMAKE_OSX_ARCHITECTURES is still relevant for the Mac app.
{ cd "$PKGDIR/build_arm64" \
    && cmake \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,-ld_classic" \
        -DWITH_GETTEXT=OFF \
        -DRust_CARGO_TARGET=aarch64-apple-darwin \
        -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
        -DFISH_USE_SYSTEM_PCRE2=OFF \
        "$SRC_DIR" \
    && env $ARM64_DEPLOY_TARGET make VERBOSE=1 -j 12 \
    && env DESTDIR="$PKGDIR/root/" $ARM64_DEPLOY_TARGET make install;
}

# Build for x86-64 but do not install; instead we will make some fat binaries inside the root.
# Set RUST_VERSION_X86_64 to the last version of Rust that supports macOS 10.9.
{ cd "$PKGDIR/build_x86_64" \
    && cmake \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,-ld_classic" \
        -DWITH_GETTEXT=OFF \
        -DRust_TOOLCHAIN="$RUST_VERSION_X86_64" \
        -DRust_CARGO_TARGET=x86_64-apple-darwin \
        -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
        -DFISH_USE_SYSTEM_PCRE2=OFF "$SRC_DIR" \
    && env $X86_64_DEPLOY_TARGET make VERBOSE=1 -j 12; }

# Fatten them up.
for FILE in "$PKGDIR"/root/usr/local/bin/*; do
    X86_FILE="$PKGDIR/build_x86_64/$(basename "$FILE")"
    rcodesign macho-universal-create --output "$FILE" "$FILE" "$X86_FILE"
    chmod 755 "$FILE"
done

if test -n "$SIGN"; then
    echo "Signing executables"
    ARGS=(
        --p12-file "$P12_APP_FILE"
        --p12-password "$P12_PASSWORD"
        --code-signature-flags runtime
        --for-notarization
    )
    if [ -n "$ENTITLEMENTS_FILE" ]; then
        ARGS+=(--entitlements-xml-file "$ENTITLEMENTS_FILE")
    fi
    for FILE in "$PKGDIR"/root/usr/local/bin/*; do
        (set +x; rcodesign sign "${ARGS[@]}" "$FILE")
    done
fi

pkgbuild --scripts "$SRC_DIR/build_tools/osx_package_scripts" --root "$PKGDIR/root/" --identifier 'com.ridiculousfish.fish-shell-pkg' --version "$VERSION" "$PKGDIR/intermediates/fish.pkg"
productbuild  --package-path "$PKGDIR/intermediates" --distribution "$SRC_DIR/build_tools/osx_distribution.xml" --resources "$SRC_DIR/build_tools/osx_package_resources/" "$OUTPUT_PATH/fish-$VERSION.pkg"

if test -n "$SIGN"; then
    echo "Signing installer"
    ARGS=(
        --p12-file "$P12_INSTALL_FILE"
        --p12-password "$P12_PASSWORD"
        --code-signature-flags runtime
        --for-notarization
    )
    (set +x; rcodesign sign "${ARGS[@]}" "$OUTPUT_PATH/fish-$VERSION.pkg")
fi

# Make the app
(cd "$PKGDIR/build_arm64" && env $ARM64_DEPLOY_TARGET make -j 12 fish_macapp)
(cd "$PKGDIR/build_x86_64" && env $X86_64_DEPLOY_TARGET make -j 12 fish_macapp)

# Make the app's /usr/local/bin binaries universal. Note fish.app/Contents/MacOS/fish already is, courtesy of CMake.
cd "$PKGDIR/build_arm64"
for FILE in fish.app/Contents/Resources/base/usr/local/bin/*; do
    X86_FILE="$PKGDIR/build_x86_64/fish.app/Contents/Resources/base/usr/local/bin/$(basename "$FILE")"
    rcodesign macho-universal-create --output "$FILE" "$FILE" "$X86_FILE"

    # macho-universal-create screws up the permissions.
    chmod 755 "$FILE"
done

if test -n "$SIGN"; then
    echo "Signing app"
    ARGS=(
        --p12-file "$P12_APP_FILE"
        --p12-password "$P12_PASSWORD"
        --code-signature-flags runtime
        --for-notarization
    )
    if [ -n "$ENTITLEMENTS_FILE" ]; then
        ARGS+=(--entitlements-xml-file "$ENTITLEMENTS_FILE")
    fi
    (set +x; rcodesign sign "${ARGS[@]}" "fish.app")

fi

cp -R "fish.app" "$OUTPUT_PATH/fish-$VERSION.app"
cd "$OUTPUT_PATH"

# Maybe notarize.
if test -n "$NOTARIZE"; then
    echo "Notarizing"
    rcodesign notarize --staple --wait --max-wait-seconds 1800 --api-key-file "$API_KEY_FILE" "$OUTPUT_PATH/fish-$VERSION.pkg"
    rcodesign notarize --staple --wait --max-wait-seconds 1800 --api-key-file "$API_KEY_FILE" "$OUTPUT_PATH/fish-$VERSION.app"
fi

# Zip it up.
zip -r "fish-$VERSION.app.zip" "fish-$VERSION.app" && rm -Rf "fish-$VERSION.app"

rm -rf "$PKGDIR"
