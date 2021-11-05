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

For a comprehensive overview of fish's scripting language, see :ref:`The Fish Language <language>`.

For information on using fish interactively, see :ref:`Interactive use <interactive>`.

If you need to install fish first, read on, the rest of this document will tell you how to get, install and configure fish.

Installation
============

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

Default Shell
-------------

To make fish your default shell:

- Add the line ``/usr/local/bin/fish`` to ``/etc/shells``.
- Change your default shell with ``chsh -s /usr/local/bin/fish``.

For detailed instructions see :ref:`Switching to fish <switching-to-fish>`.

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

For a script written in another language, just replace ``/bin/bash`` with the interpreter for that language (for example: ``/usr/bin/python`` for a python script, or ``/usr/local/bin/fish`` for a fish script).

This line is only needed when scripts are executed without specifying the interpreter. For functions inside fish or when executing a script with ``fish /path/to/script``, a shebang is not required (but it doesn't hurt!).

Configuration
=============

To store configuration write it to a file called ``~/.config/fish/config.fish``.

``.fish`` scripts in ``~/.config/fish/conf.d/`` are also automatically executed before ``config.fish``.

These files are read on the startup of every shell, whether interactive and/or if they're login shells. Use ``status --is-interactive`` and ``status --is-login`` to discriminate.

Examples:
---------

To add ``~/linux/bin`` to PATH variable when using a login shell, add this to ``~/.config/fish/config.fish`` file::

    if status --is-login
        set -gx PATH $PATH ~/linux/bin
    end

This is just an exmaple; using :ref:`fish_add_path <cmd-fish_add_path>` e.g. ``fish_add_path ~/linux/bin`` which only adds the path if it isn't included yet is easier.

To run commands on exit, use an :ref:`event handler <event>` that is triggered by the exit of the shell::

    function on_exit --on-event fish_exit
        echo fish is now exiting
    end

.. _more-help:

Resources
=========

- The `GitHub page <https://github.com/fish-shell/fish-shell/>`_

- The official `Gitter channel <https://gitter.im/fish-shell/fish-shell>`_

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
   design
   relnotes
   license
