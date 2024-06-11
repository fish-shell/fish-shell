.. |Cirrus CI| image:: https://api.cirrus-ci.com/github/fish-shell/fish-shell.svg?branch=master
      :target: https://cirrus-ci.com/github/fish-shell/fish-shell
      :alt: Cirrus CI Build Status

`fish <https://fishshell.com/>`__ - the friendly interactive shell |Build Status| |Cirrus CI|
=============================================================================================

fish is a smart and user-friendly command line shell for macOS, Linux,
and the rest of the family. fish includes features like syntax
highlighting, autosuggest-as-you-type, and fancy tab completions that
just work, with no configuration required.

For downloads, screenshots and more, go to https://fishshell.com/.

Quick Start
-----------

fish generally works like other shells, like bash or zsh. A few
important differences can be found at
https://fishshell.com/docs/current/tutorial.html by searching for the
magic phrase “unlike other shells”.

Detailed user documentation is available by running ``help`` within
fish, and also at https://fishshell.com/docs/current/index.html

Getting fish
------------

macOS
~~~~~

fish can be installed:

-  using `Homebrew <http://brew.sh/>`__: ``brew install fish``
-  using `MacPorts <https://www.macports.org/>`__:
   ``sudo port install fish``
-  using the `installer from fishshell.com <https://fishshell.com/>`__
-  as a `standalone app from fishshell.com <https://fishshell.com/>`__

Note: The minimum supported macOS version is 10.10 "Yosemite".

Packages for Linux
~~~~~~~~~~~~~~~~~~

Packages for Debian, Fedora, openSUSE, and Red Hat Enterprise
Linux/CentOS are available from the `openSUSE Build
Service <https://software.opensuse.org/download.html?project=shells%3Afish&package=fish>`__.

Packages for Ubuntu are available from the `fish
PPA <https://launchpad.net/~fish-shell/+archive/ubuntu/release-3>`__,
and can be installed using the following commands:

::

   sudo apt-add-repository ppa:fish-shell/release-3
   sudo apt update
   sudo apt install fish

Instructions for other distributions may be found at
`fishshell.com <https://fishshell.com>`__.

Windows
~~~~~~~

-  On Windows 10/11, fish can be installed under the WSL Windows Subsystem
   for Linux with the instructions for the appropriate distribution
   listed above under “Packages for Linux”, or from source with the
   instructions below.
-  Fish can also be installed on all versions of Windows using
   `Cygwin <https://cygwin.com/>`__ (from the **Shells** category).

Building from source
~~~~~~~~~~~~~~~~~~~~

If packages are not available for your platform, GPG-signed tarballs are
available from `fishshell.com <https://fishshell.com/>`__ and
`fish-shell on
GitHub <https://github.com/fish-shell/fish-shell/releases>`__. See the
`Building <#building>`__ section for instructions.

Running fish
------------

Once installed, run ``fish`` from your current shell to try fish out!

Dependencies
~~~~~~~~~~~~

Running fish requires:

-  A terminfo database, typically from curses or ncurses (preinstalled on most \*nix systems) - this needs to be the directory tree format, not the "hashed" database.
   If this is unavailable, fish uses an included xterm-256color definition.
-  some common \*nix system utilities (currently ``mktemp``), in
   addition to the basic POSIX utilities (``cat``, ``cut``, ``dirname``,
   ``file``, ``ls``, ``mkdir``, ``mkfifo``, ``rm``, ``sort``, ``tee``, ``tr``,
   ``uname`` and ``sed`` at least, but the full coreutils plus ``find`` and
   ``awk`` is preferred)
-  The gettext library, if compiled with
   translation support

The following optional features also have specific requirements:

-  builtin commands that have the ``--help`` option or print usage
   messages require ``nroff`` or ``mandoc`` for
   display
-  automated completion generation from manual pages requires Python 3.5+
-  the ``fish_config`` web configuration tool requires Python 3.5+ and a web browser
-  system clipboard integration (with the default Ctrl-V and Ctrl-X
   bindings) require either the ``xsel``, ``xclip``,
   ``wl-copy``/``wl-paste`` or ``pbcopy``/``pbpaste`` utilities
-  full completions for ``yarn`` and ``npm`` require the
   ``all-the-package-names`` NPM module
-  ``colorls`` is used, if installed, to add color when running ``ls`` on platforms
   that do not have color support (such as OpenBSD)

Building
--------

.. _dependencies-1:

Dependencies
~~~~~~~~~~~~

Compiling fish from a tarball requires:

-  a C++11 compiler (g++ 4.8 or later, or clang 3.3 or later)
-  CMake (version 3.5 or later)
-  PCRE2 (headers and libraries) - optional, this will be downloaded if missing
-  gettext (headers and libraries) - optional, for translation support

Sphinx is also optionally required to build the documentation from a
cloned git repository.

Additionally, running the test suite requires Python 3.5+ and the pexpect package.

Dependencies, git master
~~~~~~~~~~~~~~~~~~~~~~~~

Building from git master currently requires:

-  Rust (version 1.70 or later)
-  CMake (version 3.19 or later)
-  a C compiler (for system feature detection and the test helper binary)
-  PCRE2 (headers and libraries) - optional, this will be downloaded if missing
-  gettext (headers and libraries) - optional, for translation support
-  an Internet connection, as other dependencies will be downloaded automatically


Building from source (all platforms) - Makefile generator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To install into ``/usr/local``, run:

.. code:: bash

   mkdir build; cd build
   cmake ..
   make
   sudo make install

The install directory can be changed using the
``-DCMAKE_INSTALL_PREFIX`` parameter for ``cmake``.

Build options
~~~~~~~~~~~~~

In addition to the normal CMake build options (like ``CMAKE_INSTALL_PREFIX``), fish has some other options available to customize it.

- BUILD_DOCS=ON|OFF - whether to build the documentation. This is automatically set to OFF when Sphinx isn't installed.
- INSTALL_DOCS=ON|OFF - whether to install the docs. This is automatically set to on when BUILD_DOCS is or prebuilt documentation is available (like when building in-tree from a tarball).
- FISH_USE_SYSTEM_PCRE2=ON|OFF - whether to use an installed pcre2. This is normally autodetected.
- MAC_CODESIGN_ID=String|OFF - the codesign ID to use on Mac, or "OFF" to disable codesigning.
- WITH_GETTEXT=ON|OFF - whether to build with gettext support for translations.

Note that fish does *not* support static linking and will attempt to error out if it detects it.

Help, it didn’t build!
~~~~~~~~~~~~~~~~~~~~~~

On Debian or Ubuntu you want these packages:

::

   sudo apt install build-essential cmake libpcre2-dev gettext

On RedHat, CentOS, or Amazon EC2 everything should be preinstalled.

Contributing Changes to the Code
--------------------------------

See the `Guide for Developers <CONTRIBUTING.rst>`__.

Contact Us
----------

Questions, comments, rants and raves can be posted to the official fish
mailing list at https://lists.sourceforge.net/lists/listinfo/fish-users
or join us on our `matrix
channel <https://matrix.to/#/#fish-shell:matrix.org>`__. Or use the `fish tag
on Unix & Linux Stackexchange <https://unix.stackexchange.com/questions/tagged/fish>`__.
There is also a fish tag on Stackoverflow, but it is typically a poor fit.

Found a bug? Have an awesome idea? Please `open an
issue <https://github.com/fish-shell/fish-shell/issues/new>`__.

.. |Build Status| image:: https://github.com/fish-shell/fish-shell/workflows/make%20test/badge.svg
   :target: https://github.com/fish-shell/fish-shell/actions
