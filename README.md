[fish](http://fishshell.com/) - the friendly interactive shell [![Build Status](https://travis-ci.org/fish-shell/fish-shell.svg?branch=master)](https://travis-ci.org/fish-shell/fish-shell)
================================================

fish is a smart and user-friendly command line shell for OS X, Linux, and the rest of the family. fish includes features like syntax highlighting, autosuggest-as-you-type, and fancy tab completions that just work, with no configuration required.

For more on fish's design philosophy, see the [design document](http://fishshell.com/docs/current/design.html).

## Quick Start

fish generally works like other shells, like bash or zsh. A few important differences can be found at <http://fishshell.com/docs/current/tutorial.html> by searching for the magic phrase "unlike other shells".

Detailed user documentation is available by running `help` within fish, and also at <http://fishshell.com/docs/current/index.html>

## Building

fish is written in a sane subset of C++98, with a few components from C++TR1. It builds successfully with g++ 4.2 or later, and with clang. It also will build as C++11.

fish can be built using autotools or Xcode. autoconf 2.60 or later is required.

fish depends on a curses implementation, such as ncurses. The headers and libraries are required for building.

fish requires gettext for translation support.

Building the documentation requires Doxygen 1.8.7 or newer.

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

    sudo apt-get install build-essential ncurses-dev libncurses5-dev gettext autoconf

On RedHat, CentOS, or Amazon EC2:

    sudo yum install ncurses-devel

## Testing

### Travis CI Build and Test

You can have the Travis continuous integration tool automatically build and test your changes. This requires you to fork the project on GitHub or have pushed your local fish-shell repository to GitHub.

Login to [Travis CI](https://travis-ci.org/) with your GitHub account and enable your fish-shell clone. To reach that page click the plus-sign to the right of "My Repositories" on the main page for your account or go to your [profile page](https://travis-ci.org/profile/). After you do that every time you push changes to GitHub Travis will automatically build and test those changes. You'll receive an email when the tests are complete telling you whether or not any tests failed. This helps avoid being embarrassed by making a pull-request only to find you introduced a bug or failed to update a unit test. This also ensures that even if you can build and run fish on your system that it can also be built and run on other types of systems.

You'll find the configuration used to control Travis in the `.travis.yml` file.

### Running the Tests On Your Local Server

You should not build and install fish using the instructions above after
making changs until you've run the tests. You may or may not need to create an
appropriate `Makefile` by running the following one time:

    autoconf
    ./configure

To run the unit tests:

    make test

Note: These instructions will work on Mac OS X as well as Linux but do require that you've used something like [Homebrew](http://brew.sh/) to install autoconf and related tools.

## Runtime Dependencies

fish requires a curses implementation, such as ncurses, to run.

fish requires a number of utilities to operate, which should be present on any Unix, GNU/Linux or OS X system. These include (but are not limited to) hostname, grep, awk, sed, which, and getopt. fish also requires the bc program.

Translation support requires the gettext program.

Some optional features of fish, such as the manual page completion parser and the web configuration tool, require Python.

In order to generate completions from man pages compressed with either lzma or xz, you may need to install an extra Python package. Python versions prior to 2.6 are not supported.  For Python versions 2.6 to 3.2 you need to install the module `backports.lzma`.  How to install it depends on your system and how you installed Python.  Most Linux distributions should include it as a package named `backports-lzma` (or similar).  From version 3.3 onwards, Python already includes the required module.

## Packages for Linux

Instructions on how to find builds for several Linux distros are at <https://github.com/fish-shell/fish-shell/wiki/Nightly-builds>

## Switching to fish

If you wish to use fish as your default shell, use the following command:

	chsh -s /usr/local/bin/fish

chsh will prompt you for your password, and change your default shell. Substitute "/usr/local/bin/fish" with whatever path to fish is in your /etc/shells file.

To switch your default shell back, you can run:

	chsh -s /bin/bash

Substitute /bin/bash with /bin/tcsh or /bin/zsh as appropriate.

## Contact Us

Questions, comments, rants and raves can be posted to the official fish mailing list at <https://lists.sourceforge.net/lists/listinfo/fish-users> or join us on our [gitter.im channel](https://gitter.im/fish-shell/fish-shell) or IRC channel [#fish at irc.oftc.net](https://webchat.oftc.net/?channels=fish). Or use the [fish tag on Stackoverflow](https://stackoverflow.com/questions/tagged/fish).

Found a bug? Have an awesome idea? Please open an issue on this github page.
