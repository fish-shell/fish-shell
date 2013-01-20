[fish](http://ridiculousfish.com/shell/) - the friendly interactive shell
================================================

fish is a smart and user-friendly command line shell for OS X, Linux, and the rest of the family. fish includes features like syntax highlighting, autosuggest-as-you-type, and fancy tab completions that just work, with no configuration required.

For more on fish's design philosophy, see the [design document](http://ridiculousfish.com/shell/user_doc/html/design.html).

## Quick Start

fish generally works like other shells, like bash or zsh. A few important differences are documented at <http://ridiculousfish.com/shell/faq.html>

Detailed user documentation is available by running `help` within fish, and also at <http://ridiculousfish.com/shell/user_doc/html/>

## Building

fish can be built using autotools or Xcode.

### Autotools Build

    autoconf
    ./configure [--without-xsel]
    make [gmake on BSD]
    sudo make install

### Xcode Development Build

* Build the `base` target in Xcode
* Run the fish executable, for example, in `DerivedData/fish/Build/Products/Debug/base/bin/fish`

### Xcode Build and Install

    xcodebuild install
    sudo ditto /tmp/fish.dst /

## Packages for Linux

Nightly builds for several Linux distros can be downloaded from <http://download.opensuse.org/repositories/home:/siteshwar/>

## Contact Us

Questions, comments, rants and raves can be posted to the official fish mailing list at <https://lists.sourceforge.net/lists/listinfo/fish-users>
Found a bug? Have an awesome idea? Please open an issue on this github page.
