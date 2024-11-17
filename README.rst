neofish
=======

This is a soft fork of `fish shell <https://fishshell.com/>`_, a Unix shell for interactive use.

Our goal is to further improve the interactive shell experience, especially for users who are
already familiar with POSIX shells.

Most of the current changes add well-known syntax present in other shells. Just like ZSH, neofish
will not be strictly POSIX compatible (at least not in the default mode). For scripting, we
recommend using ``/bin/sh`` instead.

Our ``master`` branch is frequently rebased on `upstream
<https://github.com/fish-shell/fish-shell/>`_ and force-pushed. This makes it easy to both see the
net difference and to reintegrate any changes into upstream, which we're happy work on. Maybe some
features should be opt-in?

Here is an overview of the changes, at various stages of completion.
Some already integrated into upstream, some others are functional but lack polish.

- ☑ ``$()`` command substitutions
- ☑ ``$()`` in command position (to compute the command to run)
- ☑ ``{}`` compound commands
- ☑ ``()`` subshells
- ☐ ``$(())`` arithmetic expansion
- ☑ control flow like  ``if foo; then; fi`` and ``for; do; done``.
- ☑ ``foo=bar`` variable overrides
- ☑ ``foo=bar`` global variable assignments
- ☐ ``foo() { :; }`` function definitions
- ☐ POSIX-style single quotes
- ☐ POSIX-like heredocs (`cat << EOF`)
- ☑ variables like ``$?``, ``$$``, ``$#`` and ``$@``
- ☐ ``${foo#prefix}`` variable expansion
- ☐ process substitution (``<(foo)``)
- ☐ maybe add nestable and raw quoting syntax
- ☑ stop overriding ``MANPATH`` which shadows docs of POSIX utilties like ``exec``
- ☑ familiar abbreviations for modifier keys like ``bind c-x`` for ``bind ctrl-x``

Installation
------------

Compiling fish requires:

- Rust (version 1.70 or later)
- a C compiler
- Sphinx (to build documentation)

.. code:: shell

   git clone https://github.com/krobelus/neofish
   cd neofish
   cargo install --path .
   ~/.cargo/bin/fish
   # configure your terminal to run that binary

Optional Dependencies
---------------------

The following optional features also have specific requirements:

-  builtin commands that have the ``--help`` option or print usage
   messages require ``nroff`` or ``mandoc`` for display
-  automated completion generation from manual pages requires Python 3.5+
-  the ``fish_config`` web configuration tool requires Python 3.5+ and a web browser
-  system clipboard integration (with the default Ctrl-V and Ctrl-X
   bindings) require either the ``xsel``, ``xclip``,
   ``wl-copy``/``wl-paste`` or ``pbcopy``/``pbpaste`` utilities

Additionally, running the full test suite requires Python 3, tmux, and the pexpect package.
