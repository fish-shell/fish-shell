.. highlight:: fish-docs-samples
.. _intro:

Introduction
************

This is the documentation for **fish**, the **f**\ riendly **i**\ nteractive **sh**\ ell.

A shell is a program that helps you operate your computer by starting other programs. fish offers a command-line interface focused on usability and interactive use.

Unlike other shells, fish does not follow the POSIX standard, but still uses roughly the same model.

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

Installation and Start
======================

This section describes how to install, uninstall, start, and exit the fish shell. It also explains how to make fish the default shell.

Installation
------------

Up-to-date instructions for installing the latest version of fish are on the `fish homepage <https://fishshell.com/>`_.

To install the development version of fish, see the instructions on the `project's GitHub page <https://github.com/fish-shell/fish-shell>`_.

Starting and Exiting
--------------------

Once fish has been installed, open a terminal. If fish is not the default shell:

- Type ``fish`` to start a fish shell::

    > fish

- Type ``exit`` to exit a fish shell::

    > exit

Executing Bash
--------------

If fish is your default shell and you want to copy commands from the internet that are written in bash (the default shell on most systems), you can proceed in one of the following two ways:

- Use the ``bash`` command with the ``-c`` switch to read from a string::

    > bash -c 'some bash command'

- Use ``bash`` without a switch to open a bash shell you can use and ``exit`` afterward::

    > bash
    $ some bash command
    $ exit
    > _

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

A script written in ``bash`` would need a first line like this::

    #!/bin/bash

When the shell tells the kernel to execute the file, it will use the interpreter ``/bin/bash``.

For a script written in another language, just replace ``/bin/bash`` with the interpreter for that language (for example: ``/usr/bin/python`` for a python script, or ``/usr/local/bin/fish`` for a fish script).

This line is only needed when scripts are executed without specifying the interpreter. For functions inside fish or when executing a script with ``fish /path/to/script``, a shebang is not required (but it doesn't hurt!).


Where to add configuration
==========================

If you have any configuration you want to store, simply write it to a file called ``~/.config/fish/config.fish``.

If you want to split it up, you can also use files named something.fish in ``~/.config/fish/conf.d/``. Fish will automatically load these on startup, in order, before config.fish.

These files are read on the startup of every shell, whether it's interactive or a login shell or not. Use ``status --is-interactive`` and ``status --is-login`` to only do things for interactive shells or login shells.

This is a simplified answer for ordinary users, if you are a sysadmin or a developer who wants a program to integrate with fish, see :ref:`configuration` for the full scoop.

Examples:

If you want to add the directory ``~/linux/bin`` to your PATH variable when using a login shell, add this to your ``~/.config/fish/config.fish`` file::

    if status --is-login
        set -gx PATH $PATH ~/linux/bin
    end

(alternatively use :ref:`fish_add_path <cmd-fish_add_path>` like ``fish_add_path ~/linux/bin``, which only adds the path if it isn't included yet)

If you want to run a set of commands when fish exits, use an :ref:`event handler <event>` that is triggered by the exit of the shell::


    function on_exit --on-event fish_exit
        echo fish is now exiting
    end

.. _more-help:

Further help and development
============================

If you have a question not answered by this documentation, there are several avenues for help:

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
