.. highlight:: ghoti-docs-samples
.. _intro:

Introduction
************

This is the documentation for :command:`ghoti`, the **f**\ riendly **i**\ nteractive **sh**\ ell.

A shell is a program that helps you operate your computer by starting other programs. ghoti offers a command-line interface focused on usability and interactive use.

Some of the special features of ghoti are:

- **Extensive UI**: :ref:`Syntax highlighting <color>`, :ref:`autosuggestions`, :ref:`tab completion <tab-completion>` and selection lists that can be navigated and filtered.

- **No configuration needed**: ghoti is designed to be ready to use immediately, without requiring extensive configuration.

- **Easy scripting**: New :ref:`functions <syntax-function>` can be added on the fly. The syntax is easy to learn and use.

This page explains how to install and set up ghoti and where to get more information.

Where to go?
============

If this is your first time using ghoti, see the :ref:`tutorial <tutorial>`.

If you are already familiar with other shells like bash and want to see the scripting differences, see :ref:`Fish For Bash Users <ghoti_for_bash_users>`.

For a comprehensive overview of ghoti's scripting language, see :ref:`The Fish Language <language>`.

For information on using ghoti interactively, see :ref:`Interactive use <interactive>`.

If you need to install ghoti first, read on, the rest of this document will tell you how to get, install and configure ghoti.

Installation
============

This section describes how to install, uninstall, start, and exit :command:`ghoti`. It also explains how to make ghoti the default shell.

Installation
------------

Up-to-date instructions for installing the latest version of ghoti are on the `ghoti homepage <https://ghotishell.com/>`_.

To install the development version of ghoti, see the instructions on the `project's GitHub page <https://github.com/ghoti-shell/ghoti-shell>`_.

Starting and Exiting
--------------------

Once ghoti has been installed, open a terminal. If ghoti is not the default shell:

- Type :command:`ghoti` to start a shell::

    > ghoti

- Type :command:`exit` to end the session::

    > exit

.. _default-shell:

Default Shell
-------------

There are multiple ways to switch to ghoti (or any other shell) as your default.

The simplest method is to set your terminal emulator (eg GNOME Terminal, Apple's Terminal.app, or Konsole) to start ghoti directly. See its configuration and set the program to start to ``/usr/local/bin/ghoti`` (if that's where ghoti is installed - substitute another location as appropriate).

Alternatively, you can set ghoti as your login shell so that it will be started by all terminal logins, including SSH.

.. warning::

    Setting ghoti as your login shell may cause issues, such as an incorrect :envvar:`PATH`. Some operating systems, including a number of Linux distributions, require the login shell to be Bourne-compatible and to read configuration from ``/etc/profile``. ghoti may not be suitable as a login shell on these systems.

To change your login shell to ghoti:

1. Add the shell to ``/etc/shells`` with::

    > echo /usr/local/bin/ghoti | sudo tee -a /etc/shells

2. Change your default shell with::

    > chsh -s /usr/local/bin/ghoti

Again, substitute the path to ghoti for ``/usr/local/bin/ghoti`` - see ``command -s ghoti`` inside ghoti. To change it back to another shell, just substitute ``/usr/local/bin/ghoti`` with ``/bin/bash``, ``/bin/tcsh`` or ``/bin/zsh`` as appropriate in the steps above.

Uninstalling
------------

For uninstalling ghoti: see :ref:`FAQ: Uninstalling ghoti <faq-uninstalling>`.

Shebang Line
------------

Because shell scripts are written in many different languages, they need to carry information about which interpreter should be used to execute them. For this, they are expected to have a first line, the shebang line, which names the interpreter executable.

A script written in :command:`bash` would need a first line like this:
::

    #!/bin/bash

When the shell tells the kernel to execute the file, it will use the interpreter ``/bin/bash``.

For a script written in another language, just replace ``/bin/bash`` with the interpreter for that language. For example: ``/usr/bin/python`` for a python script, or ``/usr/local/bin/ghoti`` for a ghoti script, if that is where you have them installed.

If you want to share your script with others, you might want to use :command:`env` to allow for the interpreter to be installed in other locations. For example::

  #!/usr/bin/env ghoti
  echo Hello from ghoti $version

This will call ``env``, which then goes through :envvar:`PATH` to find a program called "ghoti". This makes it work, whether ghoti is installed in (for example) ``/usr/local/bin/ghoti``, ``/usr/bin/ghoti``, or ``~/.local/bin/ghoti``, as long as that directory is in :envvar:`PATH`.

The shebang line is only used when scripts are executed without specifying the interpreter. For functions inside ghoti or when executing a script with ``ghoti /path/to/script``, a shebang is not required (but it doesn't hurt!).

When executing files without an interpreter, ghoti, like other shells, tries your system shell, typically ``/bin/sh``. This is needed because some scripts are shipped without a shebang line.
       
Configuration
=============

To store configuration write it to a file called ``~/.config/ghoti/config.ghoti``.

``.ghoti`` scripts in ``~/.config/ghoti/conf.d/`` are also automatically executed before ``config.ghoti``.

These files are read on the startup of every shell, whether interactive and/or if they're login shells. Use ``status --is-interactive`` and ``status --is-login`` to do things only in interactive/login shells, respectively.

This is the short version; for a full explanation, like for sysadmins or integration for developers of other software, see :ref:`Configuration files <configuration>`.

If you want to see what you changed over ghoti's defaults, see :doc:`ghoti_delta <cmds/ghoti_delta>`.

Examples:
---------

To add ``~/linux/bin`` to PATH variable when using a login shell, add this to ``~/.config/ghoti/config.ghoti`` file::

    if status --is-login
        set -gx PATH $PATH ~/linux/bin
    end

This is just an example; using :doc:`ghoti_add_path <cmds/ghoti_add_path>` e.g. ``ghoti_add_path ~/linux/bin`` which only adds the path if it isn't included yet is easier.

To run commands on exit, use an :ref:`event handler <event>` that is triggered by the exit of the shell::

    function on_exit --on-event ghoti_exit
        echo ghoti is now exiting
    end

.. _more-help:

Resources
=========

- The `GitHub page <https://github.com/ghoti-shell/ghoti-shell/>`_

- The official `Gitter channel <https://gitter.im/ghoti-shell/ghoti-shell>`_

- The official mailing list at `ghoti-users@lists.sourceforge.net <https://lists.sourceforge.net/lists/listinfo/ghoti-users>`_

If you have an improvement for ghoti, you can submit it via the GitHub page.

.. _other_pages:

Other help pages
================
.. toctree::
   :maxdepth: 1
              
   self
   faq
   interactive
   language
   commands
   ghoti_for_bash_users
   tutorial
   completions
   design
   relnotes
   license
