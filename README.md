[fish](http://fishshell.com/) - the friendly interactive shell [![Build Status](https://travis-ci.org/fish-shell/fish-shell.png?branch=travis)](https://travis-ci.org/zanchey/fish-shell)
================================================

fish is a smart and user-friendly command line shell for OS X, Linux, and the rest of the family. fish includes features like syntax highlighting, autosuggest-as-you-type, and fancy tab completions that just work, with no configuration required.

For more on fish's design philosophy, see the [design document](http://fishshell.com/docs/2.0/design.html).

## Quick Start

fish generally works like other shells, like bash or zsh. A few important differences can be found at <http://fishshell.com/tutorial.html> by searching for magic phrase 'unlike other shells'.

Detailed user documentation is available by running `help` within fish, and also at <http://fishshell.com/docs/2.0/index.html>

## Building

fish is written in a sane subset of C++98, with a few components from C++TR1. It builds successfully with g++ 4.2 or later, and with clang. It also will build as C++11.

fish can be built using autotools or Xcode. autoconf 2.60 or later is required.

fish requires gettext for translation support.

### Autotools Build

    autoconf
    ./configure
    make [gmake on BSD]
    sudo make install

### Xcode Development Build

* Build the `base` target in Xcode
* Run the fish executable, for example, in `DerivedData/fish/Build/Products/Debug/base/bin/fish`

### Xcode Build and Install

    xcodebuild install
    sudo ditto /tmp/fish.dst /

## Help, it didn't build!

If fish reports that it could not find curses, try installing a curses development package and build again.

On Debian or Ubuntu you want:

    sudo apt-get install libncurses5-dev

on RedHat, CentOS, or Amazon EC2:

    sudo yum install ncurses-devel

## Packages for Linux

Instructions on how to find builds for several Linux distros are at <https://github.com/fish-shell/fish-shell/wiki/Nightly-builds>

## Switching to fish

If you wish to use fish as your default shell, use the following command:

	chsh -s /usr/local/bin/fish

chsh will prompt you for your password, and change your default shell.

To switch your default shell back, you can run:

	chsh -s /bin/bash

Substitute /bin/bash with /bin/tcsh or /bin/zsh as appropriate.

## Optional Dependencies

In order to generate completions from man pages compressed with either lzma or xz, you may need to install an extra Python package.

Python versions prior to 2.6 are not supported.  For Python versions 2.6 to 3.2 you need to install the module `backports.lzma`.  How to install it depends on your system and how you installed Python.  Most Linux distributions should include it as a package named `backports-lzma` (or similar).  From version 3.3 onwards, Python already includes the required module.

## Contact Us

Questions, comments, rants and raves can be posted to the official fish mailing list at <https://lists.sourceforge.net/lists/listinfo/fish-users> or join us on our IRC channel [#fish at irc.oftc.net](https://webchat.oftc.net/?channels=fish).

Found a bug? Have an awesome idea? Please open an issue on this github page.
