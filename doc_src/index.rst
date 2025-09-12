.. highlight:: fish-docs-samples
.. _intro:

Introduction
************

This is the documentation for :command:`fish`, the **f**\ riendly **i**\ nteractive **sh**\ ell.

A shell is a program that helps you operate your computer by starting other programs. fish offers a command-line interface focused on usability and interactive use.

Some of the special features of fish are:

- **Extensive UI**: :ref:`Syntax highlighting <color>`, :ref:`autosuggestions`, :ref:`tab completion <tab-completion>` and selection lists that can be navigated and filtered.

- **No configuration needed**: fish is designed to be ready to use immediately, without requiring extensive configuration.

- **Easy scripting**: New :ref:`functions <syntax-function>` can be added on the fly. The syntax is easy to learn and use.

This page explains how to install and set up fish and where to get more information.

Where to go?
============

If this is your first time using fish, see the :ref:`tutorial <tutorial>`.

If you are already familiar with other shells like bash and want to see the scripting differences, see :ref:`Fish For Bash Users <fish_for_bash_users>`.

For an overview of fish's scripting language, see :ref:`The Fish Language <language>`. If it would be useful in a script file, it's here.

For information on using fish interactively, see :ref:`Interactive use <interactive>`. If it's about key presses, syntax highlighting or anything else that needs an interactive terminal session, look here.

If you need to install fish first, read on, the rest of this document will tell you how to get, install and configure fish.

Setup
=====

This section describes how to install, uninstall, start, and exit :command:`fish`. It also explains how to make fish the default shell.

Installation
------------

Up-to-date instructions for installing the latest version of fish are on the `fish homepage <https://fishshell.com/>`_.

To install the development version of fish, see the instructions on the `project's GitHub page <https://github.com/fish-shell/fish-shell>`_.

Starting and Exiting
--------------------

Once fish has been installed, open a terminal. If fish is not the default shell:

- Type :command:`fish` to start a shell::

    > fish

- Type :command:`exit` to end the session::

    > exit

.. _default-shell:

Default Shell
-------------

There are multiple ways to switch to fish (or any other shell) as your default.

The simplest method is to set your terminal emulator (e.g. GNOME Terminal, Apple's Terminal.app, or Konsole) to start fish directly. See its configuration and set the program to start to ``/usr/local/bin/fish`` (the exact path depends on how you installed fish).

Alternatively, you can set fish as your login shell so that it will be started by all terminal logins, including SSH.

.. warning::

    Setting fish as your login shell may cause issues, such as an incorrect :envvar:`PATH`. Some operating systems, including a number of Linux distributions, require the login shell to be Bourne-compatible and to read configuration from ``/etc/profile``. fish may not be suitable as a login shell on these systems.

To change your login shell to fish:

1. Add the shell to ``/etc/shells`` with::

    > command -v fish | sudo tee -a /etc/shells

2. Change your default shell with::

    > chsh -s "$(command -v fish)"

To change it back to another shell, substitute ``fish`` with ``bash``, ``tcsh`` or ``zsh`` as appropriate in the above command.

Uninstalling
------------

For uninstalling fish: see :ref:`FAQ: Uninstalling fish <faq-uninstalling>`.

Shebang Line
------------

Because shell scripts are written in many different languages, they need to carry information about which interpreter should be used to execute them. For this, they are expected to have a first line, the shebang line, which names the interpreter executable.

A script written in :command:`bash` would need a first line like this:
::

    #!/bin/bash

When the shell tells the kernel to execute the file, it will use the interpreter ``/bin/bash``.

For a script written in another language, just replace ``/bin/bash`` with the interpreter for that language. For example: ``/usr/bin/python`` for a python script, or ``/usr/local/bin/fish`` for a fish script, if that is where you have them installed.

If you want to share your script with others, you might want to use :command:`env` to allow for the interpreter to be installed in other locations. For example::

  #!/usr/bin/env fish
  echo Hello from fish $version

This will call ``env``, which then goes through :envvar:`PATH` to find a program called "fish". This makes it work, whether fish is installed in (for example) ``/usr/local/bin/fish``, ``/usr/bin/fish``, or ``~/.local/bin/fish``, as long as that directory is in :envvar:`PATH`.

The shebang line is only used when scripts are executed without specifying the interpreter. For functions inside fish or when executing a script with ``fish /path/to/script``, a shebang is not required (but it doesn't hurt!).

When executing files without an interpreter, fish, like other shells, tries your system shell, typically ``/bin/sh``. This is needed because some scripts are shipped without a shebang line.
       
Configuration
=============

To store configuration write it to a file called ``~/.config/fish/config.fish``.

``.fish`` scripts in ``~/.config/fish/conf.d/`` are also automatically executed before ``config.fish``.

These files are read on the startup of every shell, whether interactive and/or if they're login shells. Use ``status --is-interactive`` and ``status --is-login`` to do things only in interactive/login shells, respectively.

This is the short version; for a full explanation, like for sysadmins or integration for developers of other software, see :ref:`Configuration files <configuration>`.

If you want to see what you changed over fish's defaults, see :doc:`fish_delta <cmds/fish_delta>`.

Examples:
---------

To add ``~/linux/bin`` to PATH variable when using a login shell, add this to ``~/.config/fish/config.fish`` file::

    if status --is-login
        set -gx PATH $PATH ~/linux/bin
    end

This is just an example; using :doc:`fish_add_path <cmds/fish_add_path>` e.g. ``fish_add_path ~/linux/bin`` which only adds the path if it isn't included yet is easier.

To run commands on exit, use an :ref:`event handler <event>` that is triggered by the exit of the shell::

    function on_exit --on-event fish_exit
        echo fish is now exiting
    end

.. _more-help:

Resources
=========

- The `GitHub page <https://github.com/fish-shell/fish-shell/>`_

- The official `Matrix room <https://matrix.to/#/#fish-shell:matrix.org>`_

- The official mailing list at `fish-users@lists.sourceforge.net <https://lists.sourceforge.net/lists/listinfo/fish-users>`_

If you have an improvement for fish, you can submit it via the GitHub page.

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
   fish_for_bash_users
   tutorial
   completions
   prompt
   design
   relnotes
   terminal-compatibility
   contributing
   license
