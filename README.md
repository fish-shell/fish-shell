[fish](https://fishshell.com/) - the friendly interactive shell [![Build Status](https://travis-ci.org/fish-shell/fish-shell.svg?branch=master)](https://travis-ci.org/fish-shell/fish-shell)
================================================

fish is a smart and user-friendly command line shell for macOS, Linux, and the rest of the family.
fish includes features like syntax highlighting, autosuggest-as-you-type, and fancy tab completions
that just work, with no configuration required.

For more on fish's design philosophy, see the [design document](https://fishshell.com/docs/current/design.html).

## Quick Start

fish generally works like other shells, like bash or zsh. A few important differences can be found at <https://fishshell.com/docs/current/tutorial.html> by searching for the magic phrase "unlike other shells".

Detailed user documentation is available by running `help` within fish, and also at <https://fishshell.com/docs/current/index.html>

## Getting fish

### macOS

fish can be installed:

* using [Homebrew](http://brew.sh/): `brew install fish`
* using [MacPorts](https://www.macports.org/): `sudo port install fish`
* using the [installer from fishshell.com](https://fishshell.com/)
* as a [standalone app from fishshell.com](https://fishshell.com/)

### Packages for Linux

Packages for Debian, Fedora, openSUSE, and Red Hat Enterprise Linux/CentOS are available from the
[openSUSE Build
Service](https://software.opensuse.org/download.html?project=shells%3Afish%3Arelease%3A2&package=fish).

Packages for Ubuntu are available from the [fish
PPA](https://launchpad.net/~fish-shell/+archive/ubuntu/release-2), and can be installed using the
following commands:

```
sudo apt-add-repository ppa:fish-shell/release-2
sudo apt-get update
sudo apt-get install fish
```

Instructions for other distributions may be found at [fishshell.com](https://fishshell.com).

### Windows

fish can be installed using [Cygwin](https://cygwin.com/) Setup (under the **Shells** category).

fish can be installed into Windows Subsystem for Linux using the instructions under *Packages for
Linux* for the appropriate image (eg Ubuntu).

### Building from source

If packages are not available for your platform, GPG-signed tarballs are available from
[fishshell.com](https://fishshell.com/) and [fish-shell on
GitHub](https://github.com/fish-shell/fish-shell/releases).

See the *Building* section for instructions.

## Running fish

Once installed, run `fish` from your current shell to try fish out!

### Dependencies

Running fish requires:

* a curses implementation such as ncurses (libraries and the `tput` command)
* PCRE2 library - a copy is included with fish
* gettext (library and `gettext` command), if compiled with translation support
* basic system utilities including `basename`, `cat`, `cut`, `date`, `dircolors`, `dirname`, `ls`,
  `mkdir`, `mkfifo`, `mktemp`, `rm`, `seq`, `sort`, `stat`, `stty`, `tail`, `tr`, `tty`, `uname`,
  `uniq`, `wc`, and `whoami`
* a number of common UNIX utilities:
    * `awk`
    * `bc`, the "basic calculator" program
    * `find`
    * `grep`
    * `hostname`
    * `kill`
    * `ps`
    * `sed`

The following optional features also have specific requirements:

* builtin commands that have the `--help` option or print usage messages require `nroff` and
  `ul` (manual page formatters) to do so
* completion generation from manual pages requires Python 2.7, 3.3 or greater, and possibly the
  `backports.lzma` module for Python 2.7
* the `fish_config` Web configuration tool requires Python 2.7, 3.3 or greater, and a web browser
* system clipboard integration (with the default Ctrl-V and Ctrl-X bindings) require either the
  `xsel` or `pbcopy`/`pbpaste` utilities
* prompts which support showing VCS information (Git, Mercurial or Subversion) require these
  utilities

### Switching to fish

If you wish to use fish as your default shell, use the following command:

	chsh -s /usr/local/bin/fish

chsh will prompt you for your password, and change your default shell. Substitute `/usr/local/bin/fish` with whatever path to fish is in your `/etc/shells` file.

Use the following command if you didn't already add your fish path to `/etc/shells`.

    echo /usr/local/bin/fish | sudo tee -a /etc/shells

To switch your default shell back, you can run:

	chsh -s /bin/bash

Substitute `/bin/bash` with `/bin/tcsh` or `/bin/zsh` as appropriate.

You may need to logout/login for the change (chsh) to take effect.

## Building

### Dependencies

Compiling fish requires:

* a C++11 compiler (g++ 4.8 or later, or clang 3.3 or later)
* either GNU Make (all platforms) or Xcode (macOS only)
* a curses implementation such as ncurses (headers and libraries)
* PCRE2 (headers and libraries) - a copy is included with fish
* gettext (headers and libraries) - optional, for translation support

Compiling from git (that is, not a released tarball) also requires:

* either Xcode (macOS only) or the following Autotools utilities (all platforms):
    * autoconf 2.60 or later
    * automake 1.13 or later
* Doxygen (1.8.7 or later) - optional, for documentation

### Building from source

```bash
autoreconf --no-recursive #if building from Git
./configure
make
sudo make install
```

### Xcode Development Build

* Build the `base` target in Xcode
* Run the fish executable, for example, in `DerivedData/fish/Build/Products/Debug/base/bin/fish`

### Xcode Build and Install

    xcodebuild install
    sudo ditto /tmp/fish.dst /
    sudo make install-doc

### Help, it didn't build!

If fish reports that it could not find curses, try installing a curses development package and build again.

On Debian or Ubuntu you want:

    sudo apt-get install build-essential ncurses-dev libncurses5-dev gettext autoconf

On RedHat, CentOS, or Amazon EC2:

    sudo yum install ncurses-devel

## Contributing Changes to the Code

See the [Guide for Developers](CONTRIBUTING.md).

## Contact Us

Questions, comments, rants and raves can be posted to the official fish mailing list at <https://lists.sourceforge.net/lists/listinfo/fish-users> or join us on our [gitter.im channel](https://gitter.im/fish-shell/fish-shell) or IRC channel [#fish at irc.oftc.net](https://webchat.oftc.net/?channels=fish). Or use the [fish tag on Stackoverflow](https://stackoverflow.com/questions/tagged/fish) for questions related to fish script and the [fish tag on Superuser](https://superuser.com/questions/tagged/fish) for all other questions (e.g., customizing colors, changing key bindings).

Found a bug? Have an awesome idea? Please open an issue on this github page.
