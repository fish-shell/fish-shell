###########################################
Distros on which fish should build natively
###########################################

Fish should build on some distributions without having to install software not packaged for these distributions.
This document is intended to help with deciding which versions of build software (such as compilers) we can safely use without running into difficulties on platforms where we want native builds.
Using dependencies which are not packaged for building the fish package can be annoying and should be avoided.
On the other hand, new versions of dependencies introduce desirable features, which we want to use eventually.
Tracking which dependency versions are available in distros helps with deciding when to update and to which versions.

Distributions for which we aim to support native builds
=======================================================

TODO: Define which versions are supported in a way that is stable across releases. For now, here are some suggestions.
Inspired by discussion https://github.com/fish-shell/fish-shell/discussions/11584.

* Debian stable | 13 (Trixie)
* Fedora supported releases (release N-2 is supported until 4 weeks after N is released) | 41, 42
* FreeBSD ?
* macOS ?
* OpenBSD ?
* OpenSUSE ?
* Ubuntu latest LTS release | 24.04 LTS (Noble Numbat)
* Ubuntu latest release | 25.10 (Questing Quokka)
* Any others ?


Rust (MSRV)
===========

Add new versions here as they are released, and update Rust versions if a new one is
packaged for a certain distro version.
Remove distro versions which are no longer supported.

..
    The following should be a table, but GitHub seems unable to render list-table:
    https://sublime-and-sphinx-guide.readthedocs.io/en/latest/tables.html#list-table-directive

* - Distro version
  - available Rust version
  - Reference
* - Debian 11 (Bullseye)
  - 1.78
  - https://packages.debian.org/bullseye/rustc-web
* - Debian 12 (Bookworm)
  - 1.78
  - https://packages.debian.org/bookworm/rustc-web
* - Debian 13 (Trixie)
  - 1.85
  - https://packages.debian.org/trixie/rust-all
* - Fedora 41
  - 1.90
  - https://packages.fedoraproject.org/pkgs/rust/rust/fedora-41-updates.html
* - Fedora 42
  - 1.90
  - https://packages.fedoraproject.org/pkgs/rust/rust/fedora-42-updates.html
* - Fedora 43
  - 1.90
  - https://packages.fedoraproject.org/pkgs/rust/rust/fedora-43-updates.html
* - Ubuntu 22.10 (Jammy Jellyfish)
  - 1.82
  - https://packages.ubuntu.com/jammy/rust-1.82-src
* - Ubuntu 24.04 LTS (Noble Numbat)
  - 1.82
  - https://packages.ubuntu.com/noble/rust-1.82-all
* - Ubuntu 25.04 (Plucky Puffin)
  - 1.84
  - https://packages.ubuntu.com/plucky/rust-all
* - Ubuntu 25.10 (Questing Quokka)
  - 1.85
  - https://packages.ubuntu.com/questing/rust-all

**Additional constraints**:

* Rust dropped support for macOS versions older than 10.12 with Rust 1.74: https://blog.rust-lang.org/2023/09/25/Increasing-Apple-Version-Requirements/, so fish might not work on old macOS versions if we update our MSRV to >= 1.74.

Current MSRV of fish: 1.70

Most recent Rust version available on all supported platforms: 1.78 (or 1.73 when including old
macOS)
