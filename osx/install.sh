#!/bin/sh

# Die if anything has an error
set -e

# Make sure we're run as root
scriptname=$(basename "$0")
if [ "$(id -u)" -ne 0 ]; then
     echo "${scriptname} must be run as root"
     exit 1
fi

# Set the prefix for installation
PREFIX=/usr/local

# Jump to the Resources directory
cd "$(dirname "$0")"

# Add us to the shells list
./add-shell "${PREFIX}/bin/fish"

# Ditto the base directory to the right place
ditto ./base "${PREFIX}"

# Announce our success
echo "fish has been installed under ${PREFIX}/ and added to /etc/shells (if it was not already present)"
echo "To start fish, run:"
echo "    ${PREFIX}/bin/fish"
echo "If you wish to change your default shell to fish, run:"
echo "    chsh -s ${PREFIX}/bin/fish"
echo "Enjoy!"
