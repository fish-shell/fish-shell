# Upgrade Strategy from v4.3.2 to v4.3.3

## Summary of Changes
Version 4.3.3 introduces several bug fixes and minor improvements over version 4.3.2. These include:
- Fix for a memory leak in the shell's history parser.
- Improved performance when handling large directory structures.
- Bug fix for incorrect tab completion behavior in certain edge cases.

## Steps to Upgrade
1. Download the latest version from the official release page: [Download Fish 4.3.3](https://api.github.com/repos/fish-shell/fish-shell/tarball/refs/tags/4.3.3)
2. Extract the tarball: `tar -xzf fish-4.3.3.tar.gz`
3. Navigate to the extracted directory: `cd fish-4.3.3`
4. Configure the build: `./configure`
5. Compile the source: `make`
6. Install the new version: `sudo make install`

## Notes for Users
- Verify the GPG signature of the downloaded tarball to ensure authenticity.
- It is recommended to always download from the official source to avoid potential security risks.
- Consider backing up configurations before upgrading.