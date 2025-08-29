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
PPA <https://launchpad.net/~fish-shell/+archive/ubuntu/release-4>`__,
and can be installed using the following commands:

::

   sudo apt-add-repository ppa:fish-shell/release-4
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
   `Cygwin <https://cygwin.com/>`__ or `MSYS2 <https://github.com/Berrysoft/fish-msys2>`__.

Building from source
~~~~~~~~~~~~~~~~~~~~

If packages are not available for your platform, GPG-signed tarballs are
available from `fishshell.com <https://fishshell.com/>`__ and
`fish-shell on
GitHub <https://github.com/fish-shell/fish-shell/releases>`__. See the
`Building <#building>`_ section for instructions.

Running fish
------------

Once installed, run ``fish`` from your current shell to try fish out!

Dependencies
~~~~~~~~~~~~

Running fish requires:

-  some common \*nix system utilities (currently ``mktemp``), in
   addition to the basic POSIX utilities (``cat``, ``cut``, ``dirname``,
   ``file``, ``ls``, ``mkdir``, ``mkfifo``, ``rm``, ``sh``, ``sort``, ``tee``, ``tr``,
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

Compiling fish requires:

-  Rust (version 1.70 or later)
-  CMake (version 3.15 or later)
-  a C compiler (for system feature detection and the test helper binary)
-  PCRE2 (headers and libraries) - optional, this will be downloaded if missing
-  gettext (headers and libraries) - optional, for translation support
-  an Internet connection, as other dependencies will be downloaded automatically

Sphinx is also optionally required to build the documentation from a
cloned git repository.

Additionally, running the full test suite requires Python 3.5+, tmux, and the pexpect package.

Building from source with CMake
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Rather than building from source, consider using a packaged build for your platform. Using the
steps below makes fish difficult to uninstall or upgrade. Release packages are available from the
links above, and up-to-date `development builds of fish are available for many platforms
<https://github.com/fish-shell/fish-shell/wiki/Development-builds>`__

To install into ``/usr/local``, run:

.. code:: bash

   mkdir build; cd build
   cmake ..
   cmake --build .
   sudo cmake --install .

The install directory can be changed using the
``-DCMAKE_INSTALL_PREFIX`` parameter for ``cmake``.

CMake Build options
~~~~~~~~~~~~~~~~~~~

In addition to the normal CMake build options (like ``CMAKE_INSTALL_PREFIX``), fish's CMake build has some other options available to customize it.

- Rust_COMPILER=path - the path to rustc. If not set, cmake will check $PATH and ~/.cargo/bin
- Rust_CARGO=path - the path to cargo. If not set, cmake will check $PATH and ~/.cargo/bin
- Rust_CARGO_TARGET=target - the target to pass to cargo. Set this for cross-compilation.
- BUILD_DOCS=ON|OFF - whether to build the documentation. This is automatically set to OFF when Sphinx isn't installed.
- INSTALL_DOCS=ON|OFF - whether to install the docs. This is automatically set to on when BUILD_DOCS is or prebuilt documentation is available (like when building in-tree from a tarball).
- FISH_USE_SYSTEM_PCRE2=ON|OFF - whether to use an installed pcre2. This is normally autodetected.
- MAC_CODESIGN_ID=String|OFF - the codesign ID to use on Mac, or "OFF" to disable codesigning.
- WITH_GETTEXT=ON|OFF - whether to build with gettext support for translations.
- extra_functionsdir, extra_completionsdir and extra_confdir - to compile in an additional directory to be searched for functions, completions and configuration snippets

Building fish with embedded data (experimental)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can also build fish with the data files embedded.

This will include all the datafiles like the included functions or web configuration tool in the main ``fish`` binary.

Fish will then read these right from its own binary, and print them out when needed. Some files, like the webconfig tool and the manpage completion generator, will be extracted to a temporary directory on-demand. You can list the files with ``status list-files`` and print one with ``status get-file path/to/file`` (e.g. ``status get-file functions/fish_prompt.fish`` to get the default prompt).

To install fish with embedded files, just use ``cargo``, like::

   cargo install --path /path/to/fish # if you have a git clone
   cargo install --git https://github.com/fish-shell/fish-shell --tag 4.0.0 # to build from git with a specific version
   cargo install --git https://github.com/fish-shell/fish-shell # to build the current development snapshot without cloning

This will place the binaries in ``~/.cargo/bin/``, but you can place them wherever you want.

This build won't have the HTML docs (``help`` will open the online version) or translations.

It will try to build the man pages with sphinx-build. If that is not available and you would like to include man pages, you need to install it and retrigger the build script, e.g. by setting FISH_BUILD_DOCS=1::

  FISH_BUILD_DOCS=1 cargo install --path .

Setting it to "0" disables the inclusion of man pages.

You can also link this build statically (but not against glibc) and move it to other computers.

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
