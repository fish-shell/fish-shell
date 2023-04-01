#!/bin/sh

# Die if anything has an error
set -e

# Make sure we're run as root
scriptname=$(basename "$0")
if [ "$UID" -ne 0 ]; then
     echo "${scriptname} must be run as root"
     exit 1
fi

# Set the prefix for installation
PREFIX=/usr/local

# Jump to the Resources directory
cd "$(dirname "$0")"

# Add us to the shells list
./add-shell "${PREFIX}/bin/ghoti"

# Ditto the base directory to the right place
ditto ./base "${PREFIX}"

# Announce our success
echo "ghoti has been installed under ${PREFIX}/ and added to /etc/shells (if it was not already present)"
echo "To start ghoti, run:"
echo "    ${PREFIX}/bin/ghoti"
echo "If you wish to change your default shell to ghoti, run:"
echo "    chsh -s ${PREFIX}/bin/ghoti"
echo "Enjoy!"
