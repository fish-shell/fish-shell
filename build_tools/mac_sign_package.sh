#!/usr/bin/env bash

# This codesigns a Mac .pkg (installer) file.
# Normally we could use `productsign` but that no longer produces
# installers compatible with 10.11.
# See https://github.com/fish-shell/fish-shell/issues/7656
#
# So instead we use the flow described here:
#   http://users.wfu.edu/cottrell/productsign/productsign_linux.html
#
# This script expects the following:
#
# 1. A variable $MAC_PRODUCTSIGN_CERTS_DIR pointing at a directory containing files cert00, cert01, cert02
# 2. A variable $MAC_PRODUCTSIGN_P12_FILE containing the "Mac Developer ID Installer" keychain item, exported as p12. See below.

die() { echo "$*" 1>&2 ; exit 1; }

# Exit on error.
set -e

# Our input package file.
INPUT_PKG=$(realpath $1)
test -f "$INPUT_PKG" || die "${INPUT_PKG} not a valid package"

# Find where our mac_xar_116 binary is.
XAR_116="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"/bin/mac_xar_116

# Here's what we need to be set.
test -f "${MAC_PRODUCTSIGN_P12_FILE}" || die "MAC_PRODUCTSIGN_P12_FILE not set or not a p12 file"
test -d "${MAC_PRODUCTSIGN_CERTS_DIR}" || die "MAC_PRODUCTSIGN_CERTS_DIR not set or not a directory"
test -f "${MAC_PRODUCTSIGN_CERTS_DIR}/cert00" \
    && test -f "${MAC_PRODUCTSIGN_CERTS_DIR}/cert01" \
    && test -f "${MAC_PRODUCTSIGN_CERTS_DIR}/cert02" \
    || die "MAC_PRODUCTSIGN_CERTS_DIR does not contain cert00, cert01, cert02"
test -x "${XAR_116}" || die "mac_xar_116 binary not found or executable at ${XAR_116}"

TMP=$(mktemp -d)
KEYFILE="${TMP}/key.pem"
DIGFILE="${TMP}/digestinfo.dat"
SIGFILE="${TMP}/signature.dat"

set -x

openssl pkcs12 -in "${MAC_PRODUCTSIGN_P12_FILE}" -nodes | openssl rsa -out "${KEYFILE}"
test -f "${KEYFILE}" || die "openssl did not create key.pem"
SIGSIZE=$(openssl dgst -sign "${KEYFILE}" -binary < /dev/null | wc -c | xargs)

# Prepare data for signing.
${XAR_116} --sign -f ${INPUT_PKG} --digestinfo-to-sign "${DIGFILE}" \
    --sig-size "${SIGSIZE}" \
    --cert-loc "${MAC_PRODUCTSIGN_CERTS_DIR}/cert00" \
    --cert-loc "${MAC_PRODUCTSIGN_CERTS_DIR}/cert01" \
    --cert-loc "${MAC_PRODUCTSIGN_CERTS_DIR}/cert02"

# Create the signature.
openssl rsautl -sign -inkey "${KEYFILE}" -in "${DIGFILE}" -out "${SIGFILE}"

# Add it to the archive, in place, then move it back.
${XAR_116} --inject-sig "${SIGFILE}" -f "${INPUT_PKG}"

# Remove all our junk.
rm -rf "${TMP}"

# Check the signature!
pkgutil --check-signature "${INPUT_PKG}"

# The following is taken from http://users.wfu.edu/cottrell/productsign/productsign_linux.html
# Saved here for posterity.

# Signing a Mac OS X package on Linux

# Premises

# You are a software developer who's at home on Linux but you want to produce builds of your software for other platforms, including Mac OS X.
# You've already figured out cross-compilation. And in regard to OS X you've figured out how to build a (flat) pkg file on Linux – or if not, you can do so quite quickly by looking at the bomutils doc: https://github.com/hogliux/bomutils.
# You are grudgingly willing to pay the Apple tax (the fee for becoming a registered developer) so that you can get a certificate with which to sign your package, in order that your gentle users don't get off-putting messages from Gatekeeper.
# But you're wondering how to sign your package without having to use Apple's productsign on a Mac.
# If you match on all points, we're in business! Here's the drill as I have figured it out. You will need: openssl, recent xar (see below), and one-time access to an actual Mac.

# Procedure

# Step 0: Build your program and create an OS X pkg file (xar archive). This you will do (on Linux) whenever you want to create a new release or snapshot.

# Step 1: This is a one-time step to be performed on a Mac. There may be a way around it, but I'm not aware of one. Please let me know if you're cleverer than I when it comes to certificates and all that. But anyway, follow the Apple directions for installing your developer certificate(s) on OS X, and use productsign to sign your package on the Mac – just this once! (Copy it across from Linux.) And then, before leaving the Mac, open Keychain Access and find your developer cert, the one with "Developer ID Installer" in its title (it should have a private key tucked under it). Highlight it and select "Export items" under the File menu to save as a p12 file. Copy your signed package and the exported p12 file (let's say it's called certs.p12) to your Linux box.

# Step 2: Back on Linux you're going to need a reasonably recent version of xar, specifically 1.6.1 or higher to support signing. Arch Linux installs xar 1.6.1 if you do pacman -S xar. Fedora's dnf install xar gets version 1.5, which won't do the job. I don't know about other distros, but if need be you can find the source for xar 1.6.1 at http://mackyle.github.io/xar/. Anyway, here's another one-time step: you'll extract the certs you need from the pkg file that you signed on the Mac, and the private key from the p12 file you exported from Keychain Access. (You'll need the passphrase that you set on the p12 when exporting it, so I hope you haven't forgotten that.)

# I'll assume (unimaginatively) that your package is called foo.pkg.

# # extract the certs from signed foo.pkg
# mkdir certs
# xar -f foo.pkg --extract-certs certs
# You should find certs00, certs01 and probably certs02 in the certs directory. Perhaps more.

# # extract the private key from certs.p12 (requires passphrase)
# openssl pkcs12 -in certs.p12 -nodes | openssl rsa -out key.pem
# At this point you have the materials to sign future versions of your package natively on Linux. I'll now assume that a new unsigned foo.pkg is sitting in a directory containing the key.pem generated above and also the certs subdirectory created above. So now (with many thanks to mackyle!) you do:

# PKG=foo.pkg

# # determine the size of the signature
# : | openssl dgst -sign key.pem -binary | wc -c > siglen.txt

# # prepare data for signing -- may have to adjust depending
# # on the contents of the certs subdir in your case
# xar --sign -f $PKG --digestinfo-to-sign digestinfo.dat \
#   --sig-size `cat siglen.txt` \
#   --cert-loc certs/cert00 \
#   --cert-loc certs/cert01 \
#   --cert-loc certs/cert02

# # create the signature
# openssl rsautl -sign -inkey key.pem -in digestinfo.dat \
#  -out signature.dat

# # stuff it into the archive
# xar --inject-sig signature.dat -f $PKG

# # and clean up
# rm -f signature.dat digestinfo.dat siglen.txt
# From this point on, just build your package on Linux and sign it on Linux using xar along with the certs and key that you got from the Mac.

