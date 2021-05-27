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

Further Reading
===============

If this is your first time using fish, see the :ref:`tutorial <tutorial>`.

If you are already familiar with other shells like bash and want to see the scripting differences, see :ref:`Fish For Bash Users <fish_for_bash_users>`.

For a comprehensive overview of fish's scripting language, see :ref:`The Fish Language <language>`.

For information on using fish interactively, see :ref:`Interactive use <interactive>`.

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

.. _initialization:

Initialization files
====================

On startup, Fish evaluates a number of configuration files, which can be used to control the behavior of the shell. The location of these is controlled by a number of environment variables, and their default or usual location is given below.

Configuration files are evaluated in the following order:

- Configuration shipped with fish, which should not be edited, in ``$__fish_data_dir/config.fish`` (usually ``/usr/share/fish/config.fish``).

- Configuration snippets in files ending in ``.fish``, in the directories:

  - ``$__fish_config_dir/conf.d`` (by default, ``~/.config/fish/conf.d/``)
  - ``$__fish_sysconf_dir/conf.d`` (by default, ``/etc/fish/conf.d/``)
  - Directories for third-party software vendors to ship their own configuration snippets for their software. Fish searches the directories in the ``XDG_DATA_DIRS`` environment variable for a ``fish/vendor_conf.d`` directory; if this variable is not defined, the default is usually to search ``/usr/share/fish/vendor_conf.d`` and ``/usr/local/share/fish/vendor_conf.d``

  If there are multiple files with the same name in these directories, only the first will be executed.
  They are executed in order of their filename, sorted (like globs) in a natural order (i.e. "01" sorts before "2").

- System-wide configuration files, where administrators can include initialization that should be run for all users on the system - similar to ``/etc/profile`` for POSIX-style shells - in ``$__fish_sysconf_dir`` (usually ``/etc/fish/config.fish``).
- User initialization, usually in ``~/.config/fish/config.fish`` (controlled by the ``XDG_CONFIG_HOME`` environment variable, and accessible as ``$__fish_config_dir``).

These paths are controlled by parameters set at build, install, or run time, and may vary from the defaults listed above.

If you are unsure where to put your own customisations, use ``~/.config/fish/config.fish``.

``~/.config/fish/config.fish`` is sourced *after* the snippets. This is so users can copy snippets and override some of their behavior.

These files are all executed on the startup of every shell. If you want to run a command only on starting an interactive shell, use the exit status of the command ``status --is-interactive`` to determine if the shell is interactive. If you want to run a command only when using a login shell, use ``status --is-login`` instead. This will speed up the starting of non-interactive or non-login shells.

If you are developing another program, you may wish to install configuration which is run for all users of the fish shell on a system. This is discouraged; if not carefully written, they may have side-effects or slow the startup of the shell. Additionally, users of other shells will not benefit from the Fish-specific configuration. However, if they are absolutely required, you may install them to the "vendor" configuration directory. As this path may vary from system to system, the ``pkgconfig`` framework should be used to discover this path with the output of ``pkg-config --variable confdir fish``.

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
