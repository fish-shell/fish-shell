[fish](https://fishshell.com/) - the friendly interactive shell [![Build Status](https://travis-ci.org/fish-shell/fish-shell.svg?branch=master)](https://travis-ci.org/fish-shell/fish-shell)
================================================

fish is a smart and user-friendly command line shell for OS X, Linux, and the rest of the family. fish includes features like syntax highlighting, autosuggest-as-you-type, and fancy tab completions that just work, with no configuration required.

For more on fish's design philosophy, see the [design document](https://fishshell.com/docs/current/design.html).

## Quick Start

fish generally works like other shells, like bash or zsh. A few important differences can be found at <https://fishshell.com/docs/current/tutorial.html> by searching for the magic phrase "unlike other shells".

Detailed user documentation is available by running `help` within fish, and also at <https://fishshell.com/docs/current/index.html>

## Building

### Dependencies

Compiling fish requires:

* a C++11 compiler. It builds successfully with g++ 4.8 or later, or with clang 3.3 or later. fish can be built using autotools or Xcode. autoconf 2.60 or later, as well as automake 1.13 or later, are required to build from git versions, but these are not required to build from the officially released tarballs.
* the headers and libraries of a curses implementation such as ncurses.
* gettext, for translation support.
* Doxygen 1.8.7 or newer, for building the documentation.

At runtime, fish requires:

* a curses implementation, such as ncurses.
* gettext, if you need the localizations.
* PCRE2, due to the regular expression support contained in the `string` builtin. A copy is included with the source code, and will be used automatically if it does not already exist on your system.
* a number of common UNIX utilities: coreutils (at least ls, seq, rm, mktemp), hostname, grep, awk, sed, and getopt.
* bc, the "basic calculator" program.

Optional functionality should degrade gracefully. (If fish ever threw a syntax error or a core dump due to missing dependencies for an optional feature, it would be a bug in fish and should be reported on our GitHub.) Optional features and dependencies include:

* dynamically generating usage tips for builtin functions, which requires man, apropos, nroff, and ul.
* manual page parsing for auto-completion for external commands, which also requires the above man tools, plus Python 2.6+ or Python 3.3+. Python 2 will also need the backports.lzma package; Python 3.3+ should include the required module by default.
* the web configuration tool, which requires Python 2.6+ or 3.3+.
* the fish_clipboard_* functions (bound to \cv and \cx), which require xsel or pbcopy/pbpaste.

fish can also show VCS information in the prompt, when the current working directory is in a VCS-tracked project, with support for Git, Mercurial, and Subversion. This is one of the weakest dependencies, though: if you don't use git, for example, you don't need git information in the prompt, and if you do use git, you already have it. Similarly, fish comes with a variety of auto-completion scripts to help with command-line usage of a variety of applications; these scripts call the application in question, but you should only need these applications if you use them.

### Autotools Build

    autoreconf --no-recursive [if building from Git]
    ./configure
    make [gmake on BSD]
    sudo make install [sudo gmake install on BSD]

### Xcode Development Build

* Build the `base` target in Xcode
* Run the fish executable, for example, in `DerivedData/fish/Build/Products/Debug/base/bin/fish`

### Xcode Build and Install

    xcodebuild install
    sudo ditto /tmp/fish.dst /
    sudo make install-doc

## Help, it didn't build!

If fish reports that it could not find curses, try installing a curses development package and build again.

On Debian or Ubuntu you want:

    sudo apt-get install build-essential ncurses-dev libncurses5-dev gettext autoconf

On RedHat, CentOS, or Amazon EC2:

    sudo yum install ncurses-devel

## Packages for Linux

Instructions on how to find builds for several Linux distros are at <https://github.com/fish-shell/fish-shell/wiki/Nightly-builds>

## Switching to fish

If you wish to use fish as your default shell, use the following command:

	chsh -s /usr/local/bin/fish

chsh will prompt you for your password, and change your default shell. Substitute "/usr/local/bin/fish" with whatever path to fish is in your /etc/shells file.

Use the following command if you didn't already add your fish path to /etc/shells.

    echo /usr/local/bin/fish | sudo tee -a /etc/shells

To switch your default shell back, you can run:

	chsh -s /bin/bash

Substitute /bin/bash with /bin/tcsh or /bin/zsh as appropriate.

You may need to logout/login for the change (chsh) to take effect.

## Contributing Changes to the Code

See the [Guide for Developers](CONTRIBUTING.md).

## Contact Us

Questions, comments, rants and raves can be posted to the official fish mailing list at <https://lists.sourceforge.net/lists/listinfo/fish-users> or join us on our [gitter.im channel](https://gitter.im/fish-shell/fish-shell) or IRC channel [#fish at irc.oftc.net](https://webchat.oftc.net/?channels=fish). Or use the [fish tag on Stackoverflow](https://stackoverflow.com/questions/tagged/fish) for questions related to fish script and the [fish tag on Superuser](https://superuser.com/questions/tagged/fish) for all other questions (e.g., customizing colors, changing key bindings).

Found a bug? Have an awesome idea? Please open an issue on this github page.
