.. |Cirrus CI| image:: https://api.cirrus-ci.com/github/ghoti-shell/ghoti-shell.svg?branch=master
      :target: https://cirrus-ci.com/github/ghoti-shell/ghoti-shell
      :alt: Cirrus CI Build Status

`ghoti <https://ghotishell.com/>`__ - the friendly interactive shell |Build Status| |Cirrus CI|
=================================================================================

ghoti is a smart and user-friendly command line shell for macOS, Linux,
and the rest of the family. ghoti includes features like syntax
highlighting, autosuggest-as-you-type, and fancy tab completions that
just work, with no configuration required.

For downloads, screenshots and more, go to https://ghotishell.com/.

Quick Start
-----------

ghoti generally works like other shells, like bash or zsh. A few
important differences can be found at
https://ghotishell.com/docs/current/tutorial.html by searching for the
magic phrase “unlike other shells”.

Detailed user documentation is available by running ``help`` within
ghoti, and also at https://ghotishell.com/docs/current/index.html

Getting ghoti
------------

macOS
~~~~~

ghoti can be installed:

-  using `Homebrew <http://brew.sh/>`__: ``brew install ghoti``
-  using `MacPorts <https://www.macports.org/>`__:
   ``sudo port install ghoti``
-  using the `installer from ghotishell.com <https://ghotishell.com/>`__
-  as a `standalone app from ghotishell.com <https://ghotishell.com/>`__

Note: The minimum supported macOS version is 10.10 "Yosemite".

Packages for Linux
~~~~~~~~~~~~~~~~~~

Packages for Debian, Fedora, openSUSE, and Red Hat Enterprise
Linux/CentOS are available from the `openSUSE Build
Service <https://software.opensuse.org/download.html?project=shells%3Aghoti&package=ghoti>`__.

Packages for Ubuntu are available from the `ghoti
PPA <https://launchpad.net/~ghoti-shell/+archive/ubuntu/release-3>`__,
and can be installed using the following commands:

::

   sudo apt-add-repository ppa:ghoti-shell/release-3
   sudo apt update
   sudo apt install ghoti

Instructions for other distributions may be found at
`ghotishell.com <https://ghotishell.com>`__.

Windows
~~~~~~~

-  On Windows 10, ghoti can be installed under the WSL Windows Subsystem
   for Linux with the instructions for the appropriate distribution
   listed above under “Packages for Linux”, or from source with the
   instructions below.
-  Fish can also be installed on all versions of Windows using
   `Cygwin <https://cygwin.com/>`__ (from the **Shells** category).

Building from source
~~~~~~~~~~~~~~~~~~~~

If packages are not available for your platform, GPG-signed tarballs are
available from `ghotishell.com <https://ghotishell.com/>`__ and
`ghoti-shell on
GitHub <https://github.com/ghoti-shell/ghoti-shell/releases>`__. See the
`Building <#building>`__ section for instructions.

Running ghoti
------------

Once installed, run ``ghoti`` from your current shell to try ghoti out!

Dependencies
~~~~~~~~~~~~

Running ghoti requires:

-  curses or ncurses (preinstalled on most \*nix systems)
-  some common \*nix system utilities (currently ``mktemp``), in
   addition to the basic POSIX utilities (``cat``, ``cut``, ``dirname``,
   ``ls``, ``mkdir``, ``mkfifo``, ``rm``, ``sort``, ``tee``, ``tr``,
   ``uname`` and ``sed`` at least, but the full coreutils plus ``find`` and
   ``awk`` is preferred)
-  The gettext library, if compiled with
   translation support

The following optional features also have specific requirements:

-  builtin commands that have the ``--help`` option or print usage
   messages require ``nroff`` or ``mandoc`` for
   display
-  automated completion generation from manual pages requires Python 3.5+
-  the ``ghoti_config`` web configuration tool requires Python 3.5+ and a web browser
-  system clipboard integration (with the default Ctrl-V and Ctrl-X
   bindings) require either the ``xsel``, ``xclip``,
   ``wl-copy``/``wl-paste`` or ``pbcopy``/``pbpaste`` utilities
-  full completions for ``yarn`` and ``npm`` require the
   ``all-the-package-names`` NPM module
-  ``colorls`` is used, if installed, to add color when running ``ls`` on platforms
   that do not have color support (such as OpenBSD)

Switching to ghoti
~~~~~~~~~~~~~~~~~

If you wish to use ghoti as your default shell, use the following
command:

::

   chsh -s /usr/local/bin/ghoti

``chsh`` will prompt you for your password and change your default
shell. (Substitute ``/usr/local/bin/ghoti`` with whatever path ghoti was
installed to, if it differs.) Log out, then log in again for the changes
to take effect.

Use the following command if ghoti isn’t already added to ``/etc/shells``
to permit ghoti to be your login shell:

::

   echo /usr/local/bin/ghoti | sudo tee -a /etc/shells

To switch your default shell back, you can run ``chsh -s /bin/bash``
(substituting ``/bin/bash`` with ``/bin/tcsh`` or ``/bin/zsh`` as
appropriate).

Building
--------

.. _dependencies-1:

Dependencies
~~~~~~~~~~~~

Compiling ghoti requires:

-  Rust (version 1.67 or later)
-  a C++11 compiler (g++ 4.8 or later, or clang 3.3 or later)
-  CMake (version 3.5 or later)
-  a curses implementation such as ncurses (headers and libraries)
-  PCRE2 (headers and libraries) - optional, this will be downloaded if missing
-  gettext (headers and libraries) - optional, for translation support

Sphinx is also optionally required to build the documentation from a
cloned git repository.

Additionally, running the test suite requires Python 3.5+ and the pexpect package.

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

Building from source (macOS) - Xcode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Note: The minimum supported macOS version is 10.10 "Yosemite".

.. code:: bash

   mkdir build; cd build
   cmake .. -G Xcode

An Xcode project will now be available in the ``build`` subdirectory.
You can open it with Xcode, or run the following to build and install in
``/usr/local``:

.. code:: bash

   xcodebuild
   xcodebuild -scheme install

The install directory can be changed using the
``-DCMAKE_INSTALL_PREFIX`` parameter for ``cmake``.

Build options
~~~~~~~~~~~~~

In addition to the normal cmake build options (like ``CMAKE_INSTALL_PREFIX``), ghoti has some other options available to customize it.

- BUILD_DOCS=ON|OFF - whether to build the documentation. This is automatically set to OFF when sphinx isn't installed.
- INSTALL_DOCS=ON|OFF - whether to install the docs. This is automatically set to on when BUILD_DOCS is or prebuilt documentation is available (like when building in-tree from a tarball).
- FISH_USE_SYSTEM_PCRE2=ON|OFF - whether to use an installed pcre2. This is normally autodetected.
- MAC_CODESIGN_ID=String|OFF - the codesign ID to use on Mac, or "OFF" to disable codesigning.
- WITH_GETTEXT=ON|OFF - whether to build with gettext support for translations.

Note that ghoti does *not* support static linking and will attempt to error out if it detects it.

Help, it didn’t build!
~~~~~~~~~~~~~~~~~~~~~~

If ghoti reports that it could not find curses, try installing a curses
development package and build again.

On Debian or Ubuntu you want:

::

   sudo apt install build-essential cmake ncurses-dev libncurses5-dev libpcre2-dev gettext

On RedHat, CentOS, or Amazon EC2:

::

   sudo yum install ncurses-devel

Contributing Changes to the Code
--------------------------------

See the `Guide for Developers <CONTRIBUTING.rst>`__.

Contact Us
----------

Questions, comments, rants and raves can be posted to the official ghoti
mailing list at https://lists.sourceforge.net/lists/listinfo/ghoti-users
or join us on our `gitter.im
channel <https://gitter.im/ghoti-shell/ghoti-shell>`__. Or use the `ghoti tag
on Unix & Linux Stackexchange <https://unix.stackexchange.com/questions/tagged/ghoti>`__.
There is also a ghoti tag on Stackoverflow, but it is typically a poor fit.

Found a bug? Have an awesome idea? Please `open an
issue <https://github.com/ghoti-shell/ghoti-shell/issues/new>`__.

.. |Build Status| image:: https://github.com/ghoti-shell/ghoti-shell/workflows/make%20test/badge.svg
   :target: https://github.com/ghoti-shell/ghoti-shell/actions
