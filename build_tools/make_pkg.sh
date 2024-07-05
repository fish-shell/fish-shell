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
  echo "  -j <API_KEY.JSON>             Path to JSON file generated with `rcodesign encode-app-store-connect-api-key` (required for notarization)"
  echo
  exit 1
}

set -x
set -e

SIGN=
NOTARIZE=

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

if [ -n "$SIGN" ] && ([ -z "$P12_APP_FILE" ] || [-z "$P12_INSTALL_FILE"] || [ -z "$P12_PASSWORD" ]); then
  usage
fi

if [ -n "$NOTARIZE" ] && [ -z "$API_KEY_FILE" ]; then
  usage
fi

VERSION=$(git describe --always --dirty 2>/dev/null)
if test -z "$VERSION" ; then
  echo "Could not get version from git"
  if test -f version; then
    VERSION=$(cat version)
  fi
fi

echo "Version is $VERSION"

PKGDIR=$(mktemp -d)
echo "$PKGDIR"

SRC_DIR=$PWD
OUTPUT_PATH=${FISH_ARTEFACT_PATH:-~/fish_built}

mkdir -p "$PKGDIR/build" "$PKGDIR/root" "$PKGDIR/intermediates" "$PKGDIR/dst"

# Pass FISH_USE_SYSTEM_PCRE2=OFF because a system PCRE2 on macOS will not be signed by fish,
# and will probably not be built universal, so the package will fail to validate/run on other systems.
{ cd "$PKGDIR/build" && cmake -DMAC_INJECT_GET_TASK_ALLOW=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXE_LINKER_FLAGS="-Wl,-ld_classic" -DWITH_GETTEXT=OFF -DFISH_USE_SYSTEM_PCRE2=OFF -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' "$SRC_DIR" && make VERBOSE=1 -j 12 && env DESTDIR="$PKGDIR/root/" make install; }

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
cd "$PKGDIR/build"
make -j 12 fish_macapp
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
mv "fish.app" "$OUTPUT_PATH/fish-$VERSION.app"
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
